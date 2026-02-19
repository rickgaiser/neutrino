/*
 * UDPRDMA - Reliable RDMA over UDP for PS2
 *
 * Implementation of reliable data transfer using Go-Back-N ARQ.
 */

#include <errno.h>
#include <thevent.h>
#include <thbase.h>
#include <stdio.h>
#include <string.h>
#include <smapregs.h>
#include <speedregs.h>
#include <dmacman.h>
#include <dev9.h>

#include "udprdma.h"
#include "ministack_udp.h"
#include "main.h"
#include "mprintf.h"


/* Event flag bits */
#define EF_RX_DISC   (1<<0)  /* Received DISCOVERY */
#define EF_RX_INFO   (1<<1)  /* Received INFORM */
#define EF_RX_ACK    (1<<2)  /* Received ACK */
#define EF_RX_NACK   (1<<3)  /* Received NACK / out-of-order */
#define EF_TIMEOUT   (1<<6)  /* Timeout occurred - bit 6 avoids WEF_CLEAR(0x10) overlap */
#define EF_RX_FIN    (1<<5)  /* Receive complete (FIN or buffer full) */
#define EF_RX_WINDOW (1<<7)  /* Window ACK needed (every N packets) */

/* Flow control: PS2 ACKs every RX_ACK_WINDOW packets,
 * server sends at most SEND_WINDOW packets ahead */
#define UDPRDMA_RX_ACK_WINDOW 6

/* Socket states */
typedef enum {
    STATE_INIT,
    STATE_DISCOVERING,
    STATE_CONNECTED,
    STATE_DISCONNECTED
} udprdma_state_t;

/* Socket structure */
struct udprdma_socket {
    /* UDP layer */
    udp_socket_t *udp_socket;

    /* Configuration */
    uint16_t port;
    uint16_t service_id;

    /* Connection state */
    udprdma_state_t state;
    uint32_t peer_ip;
    uint16_t dma_max_block_size;  /* Max DMA block size: 128 (old SPEED) or 512 (new) */

    /* TX state */
    uint16_t tx_seq_nr;         /* Next sequence number to send */
    uint16_t tx_seq_nr_acked;   /* Last acknowledged sequence number */
    uint8_t tx_retries;         /* Current retry count */

    /* RX state */
    void *rx_buffer;            /* Buffer for receiving data */
    uint32_t rx_buffer_size;    /* Size of receive buffer */
    uint32_t rx_received;       /* Bytes received so far */
    uint16_t rx_seq_nr_expected; /* Next expected sequence number */
    uint8_t rx_window_count;    /* Packets received since last window ACK */

    /* RX app header state */
    void *rx_hdr_buffer;        /* App header receive buffer (NULL = not configured) */
    uint32_t rx_hdr_size;       /* Expected header size in bytes */
    uint32_t rx_hdr_received;   /* Header bytes received so far */

    /* Pre-built packet headers */
    udprdma_pkt_disc_t pkt_disc;
    udprdma_pkt_data_t pkt_data;

    /* Synchronization */
    int event_flag;
};

/* Maximum number of sockets */
#ifndef UDPRDMA_MAX_SOCKETS
#define UDPRDMA_MAX_SOCKETS 2
#endif
static struct udprdma_socket sockets[UDPRDMA_MAX_SOCKETS];


/*
 * Timeout callback
 */
static unsigned int _timeout_cb(void *arg)
{
    struct udprdma_socket *s = (struct udprdma_socket *)arg;
    iSetEventFlag(s->event_flag, EF_TIMEOUT);
    return 0;  /* Don't repeat */
}

/*
 * DMA + PIO transfer from SMAP RX FIFO to memory buffer.
 *
 * Tries the largest DMA block size that exactly fits byte_count (512/256/128).
 * If none fit exactly, uses 128-byte blocks for the aligned portion.
 * Falls back to 64-byte blocks for data < 128 bytes.
 * Remaining bytes are PIO'd (word copy from FIFO).
 * Min DMA threshold: 64 bytes.
 */
static void _fifo_to_mem(uint16_t dma_max, void *dest, uint32_t byte_count)
{
    USE_SMAP_REGS;
    uint32_t dma_bytes = 0;
    uint32_t bcr;

    /* Try exact-fit with large blocks (most efficient, no PIO needed) */
    if (byte_count >= 512 && dma_max >= 512 && (byte_count & 511) == 0) {
        dma_bytes = byte_count;
        bcr = (byte_count >> 9) << 16 | 128;   /* 512-byte = 128-word blocks */
    } else if (byte_count >= 256 && dma_max >= 256 && (byte_count & 255) == 0) {
        dma_bytes = byte_count;
        bcr = (byte_count >> 8) << 16 | 64;    /* 256-byte = 64-word blocks */
    } else if (byte_count >= 128 && (byte_count & 127) == 0) {
        dma_bytes = byte_count;
        bcr = (byte_count >> 7) << 16 | 32;    /* 128-byte = 32-word blocks */
    }
    /* Partial fit: DMA floor portion with 128-byte blocks, PIO the rest */
    else if (byte_count >= 128) {
        dma_bytes = byte_count & ~127;
        bcr = (dma_bytes >> 7) << 16 | 32;
    }
    /* Small data: 64-byte blocks + PIO */
    else if (byte_count >= 64) {
        dma_bytes = byte_count & ~63;
        bcr = (dma_bytes >> 6) << 16 | 16;     /* 64-byte = 16-word blocks */
    }
    /* Very small (< 64): pure PIO */

    if (dma_bytes > 0)
        dev9DmaTransfer(1, dest, bcr, DMAC_TO_MEM);

    /* PIO remaining bytes (always 4-byte aligned per protocol constraint) */
    for (uint32_t i = dma_bytes; i < byte_count; i += 4)
        *(uint32_t *)((uint8_t *)dest + i) = SMAP_REG32(SMAP_R_RXFIFO_DATA);
}

/*
 * Interrupt service thread callback
 *
 * Reads packet header from SMAP FIFO and sets event flags.
 * For DATA packets, transfers payload to receive buffer via DMA + PIO.
 */
static int _ist(udp_socket_t *udp_socket, uint16_t pointer, void *arg)
{
    USE_SMAP_REGS;
    struct udprdma_socket *s = (struct udprdma_socket *)arg;
    uint32_t hdr_raw;
    udprdma_hdr_t hdr;

    /* Read UDPRDMA header at offset 0x2A (after eth+ip+udp) */
    /* Note: UDP header ends at 0x2A, but we read from 0x28 for 4-byte alignment */
    SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = pointer + 0x28;
    hdr_raw = SMAP_REG32(SMAP_R_RXFIFO_DATA);
    hdr.raw = (uint16_t)(hdr_raw >> 16);  /* Header is in upper 16 bits due to alignment */

    //M_DEBUG("_ist: type=%d seq=%d\n", hdr.packet_type, hdr.seq_nr);

    switch (hdr.packet_type) {
        case UDPRDMA_PT_DISCOVERY: {
            uint32_t disc_raw = SMAP_REG32(SMAP_R_RXFIFO_DATA);
            uint16_t service_id = disc_raw & 0xFFFF;
            M_DEBUG("_ist: DISCOVERY svc=0x%04X\n", service_id);

            if (service_id == s->service_id) {
                /* Store peer IP from IP header (source IP at offset 0x1A) */
                SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = pointer + 0x18;
                uint32_t ipa = SMAP_REG32(SMAP_R_RXFIFO_DATA);
                uint32_t ipb = SMAP_REG32(SMAP_R_RXFIFO_DATA);
                s->peer_ip = (ipb << 16) | (ipa >> 16);
                s->peer_ip = ntohl(s->peer_ip);
                SetEventFlag(s->event_flag, EF_RX_DISC);
            }
            break;
        }

        case UDPRDMA_PT_INFORM: {
            uint32_t info_raw = SMAP_REG32(SMAP_R_RXFIFO_DATA);
            uint16_t service_id = info_raw & 0xFFFF;
            M_DEBUG("_ist: INFORM svc=0x%04X\n", service_id);

            if (service_id == s->service_id) {
                /* Store peer IP from IP header (source IP at offset 0x1A) */
                SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = pointer + 0x18;
                uint32_t ipa = SMAP_REG32(SMAP_R_RXFIFO_DATA);
                uint32_t ipb = SMAP_REG32(SMAP_R_RXFIFO_DATA);
                s->peer_ip = (ipb << 16) | (ipa >> 16);
                s->peer_ip = ntohl(s->peer_ip);
                /* Next expected seq is INFORM's seq + 1 */
                s->rx_seq_nr_expected = (hdr.seq_nr + 1) & 0xFFF;
                SetEventFlag(s->event_flag, EF_RX_INFO);
            }
            break;
        }

        case UDPRDMA_PT_DATA: {
            udprdma_hdr_data_t data_hdr;
            data_hdr.raw = SMAP_REG32(SMAP_R_RXFIFO_DATA);
            //M_DEBUG("_ist: DATA seq=%d flags=0x%02X bytes=%d\n",
            //    data_hdr.seq_nr_ack, data_hdr.flags, data_hdr.data_byte_count);

            /* Update TX ACK state only from ACK packets (not NACKs,
             * which carry the expected seq_nr in seq_nr_ack) */
            if (data_hdr.flags & UDPRDMA_DF_ACK) {
                s->tx_seq_nr_acked = data_hdr.seq_nr_ack;
                SetEventFlag(s->event_flag, EF_RX_ACK);
            }

            /* Handle payload if present */
            {
                uint32_t hdr_size = data_hdr.hdr_word_count * 4;
                uint32_t payload_size = hdr_size + data_hdr.data_byte_count;

                if (payload_size > 0 && s->rx_buffer != NULL) {
                    if (hdr.seq_nr == s->rx_seq_nr_expected) {
                        /* Extract app header via PIO (first packet only) */
                        if (hdr_size > 0) {
                            if (s->rx_hdr_buffer != NULL && s->rx_hdr_received == 0) {
                                /* PIO header bytes to app header buffer */
                                for (uint32_t i = 0; i < hdr_size; i += 4)
                                    *(uint32_t *)((uint8_t *)s->rx_hdr_buffer + i) =
                                        SMAP_REG32(SMAP_R_RXFIFO_DATA);
                            } else {
                                /* No header buffer configured - skip header bytes */
                                for (uint32_t i = 0; i < hdr_size; i += 4)
                                    (void)SMAP_REG32(SMAP_R_RXFIFO_DATA);
                            }
                            s->rx_hdr_received = hdr_size;
                        }

                        /* DMA data bytes to rx buffer */
                        if (data_hdr.data_byte_count > 0) {
                            uint32_t remaining = s->rx_buffer_size - s->rx_received;
                            uint32_t xfer_size = data_hdr.data_byte_count < remaining ?
                                data_hdr.data_byte_count : remaining;

                            _fifo_to_mem(s->dma_max_block_size,
                                (uint8_t *)s->rx_buffer + s->rx_received, xfer_size);

                            s->rx_received += xfer_size;
                        }

                        s->rx_seq_nr_expected = (hdr.seq_nr + 1) & 0xFFF;

                        /* Complete when FIN received or buffer full */
                        if ((data_hdr.flags & UDPRDMA_DF_FIN) ||
                            s->rx_received >= s->rx_buffer_size) {
                            SetEventFlag(s->event_flag, EF_RX_FIN);
                        } else if (++s->rx_window_count >= UDPRDMA_RX_ACK_WINDOW) {
                            /* Flow control: signal recv to send cumulative ACK */
                            s->rx_window_count = 0;
                            SetEventFlag(s->event_flag, EF_RX_WINDOW);
                        }
                    } else {
                        /* Out of order - signal NACK needed */
                        SetEventFlag(s->event_flag, EF_RX_NACK);
                    }
                }
            }
            break;
        }

        default:
            M_DEBUG("udprdma: unknown packet type %d\n", hdr.packet_type);
            break;
    }

    return 0;
}

/*
 * Send DISCOVERY packet
 */
static void _send_discovery(struct udprdma_socket *s)
{
    s->pkt_disc.hdr.packet_type = UDPRDMA_PT_DISCOVERY;
    s->pkt_disc.hdr.seq_nr = s->tx_seq_nr;
    s->pkt_disc.disc.service_id = s->service_id;
    s->pkt_disc.disc.reserved = 0;

    udp_packet_send(s->udp_socket, (udp_packet_t *)&s->pkt_disc,
        sizeof(udprdma_hdr_t) + sizeof(udprdma_hdr_disc_t));

    s->tx_seq_nr = (s->tx_seq_nr + 1) & 0xFFF;
}

/*
 * Send INFORM packet
 */
static void _send_inform(struct udprdma_socket *s)
{
    s->pkt_disc.hdr.packet_type = UDPRDMA_PT_INFORM;
    s->pkt_disc.hdr.seq_nr = s->tx_seq_nr;
    s->pkt_disc.disc.service_id = s->service_id;
    s->pkt_disc.disc.reserved = 0;

    udp_packet_send(s->udp_socket, (udp_packet_t *)&s->pkt_disc,
        sizeof(udprdma_hdr_t) + sizeof(udprdma_hdr_disc_t));

    s->tx_seq_nr = (s->tx_seq_nr + 1) & 0xFFF;
}

/*
 * Send ACK packet (DATA with no payload)
 */
static void _send_ack(struct udprdma_socket *s, int is_ack)
{
    s->pkt_data.hdr.packet_type = UDPRDMA_PT_DATA;
    s->pkt_data.hdr.seq_nr = s->tx_seq_nr;
    s->pkt_data.data.seq_nr_ack = is_ack ?
        ((s->rx_seq_nr_expected - 1) & 0xFFF) :  /* ACK: last received */
        s->rx_seq_nr_expected;                    /* NACK: expected */
    s->pkt_data.data.flags = is_ack ? UDPRDMA_DF_ACK : 0;
    s->pkt_data.data.hdr_word_count = 0;
    s->pkt_data.data.data_byte_count = 0;

    udp_packet_send(s->udp_socket, (udp_packet_t *)&s->pkt_data,
        sizeof(udprdma_hdr_t) + sizeof(udprdma_hdr_data_t));

    /* ACK-only packets don't consume sequence numbers */
}

/*
 * Send DATA packet with payload
 * Single-packet sends always set FIN (complete transfer in one packet)
 */
static void _send_data(struct udprdma_socket *s, const void *data, uint32_t size)
{
    /* Clamp to max payload */
    if (size > UDPRDMA_MAX_PAYLOAD)
        size = UDPRDMA_MAX_PAYLOAD;

    /* Round up to 4-byte boundary for DMA alignment */
    uint32_t padded_size = (size + 3) & ~3;

    s->pkt_data.hdr.packet_type = UDPRDMA_PT_DATA;
    s->pkt_data.hdr.seq_nr = s->tx_seq_nr;
    s->pkt_data.data.seq_nr_ack = (s->rx_seq_nr_expected - 1) & 0xFFF;
    s->pkt_data.data.flags = UDPRDMA_DF_ACK | UDPRDMA_DF_FIN;  /* Single packet = FIN */
    s->pkt_data.data.hdr_word_count = 0;
    s->pkt_data.data.data_byte_count = padded_size;

    udp_packet_send_ll(s->udp_socket, (udp_packet_t *)&s->pkt_data,
        sizeof(udprdma_hdr_t) + sizeof(udprdma_hdr_data_t),
        data, padded_size);

    s->tx_seq_nr = (s->tx_seq_nr + 1) & 0xFFF;
}

/*
 * Send DATA packet with scatter-gather: app header packed into packet + separate data
 * App header is copied into pkt_data.extra, data is sent directly via DMA (zero-copy).
 */
static void _send_data_ll(struct udprdma_socket *s,
                          const void *app_hdr, uint32_t app_hdr_size,
                          const void *data, uint32_t data_size)
{
    /* Round data up to 4-byte boundary for DMA alignment */
    uint32_t padded_data_size = (data_size + 3) & ~3;

    s->pkt_data.hdr.packet_type = UDPRDMA_PT_DATA;
    s->pkt_data.hdr.seq_nr = s->tx_seq_nr;
    s->pkt_data.data.seq_nr_ack = (s->rx_seq_nr_expected - 1) & 0xFFF;
    s->pkt_data.data.flags = UDPRDMA_DF_ACK | UDPRDMA_DF_FIN;
    s->pkt_data.data.hdr_word_count = app_hdr_size / 4;
    s->pkt_data.data.data_byte_count = padded_data_size;

    /* Pack app header into pkt_data.extra (contiguous with RDMA headers) */
    memcpy(s->pkt_data.extra, app_hdr, app_hdr_size);

    udp_packet_send_ll(s->udp_socket, (udp_packet_t *)&s->pkt_data,
        sizeof(udprdma_hdr_t) + sizeof(udprdma_hdr_data_t) + app_hdr_size,
        data, padded_data_size);

    s->tx_seq_nr = (s->tx_seq_nr + 1) & 0xFFF;
}


/*
 * Public API
 */

udprdma_socket_t *udprdma_create(uint16_t port, uint16_t service_id)
{
    struct udprdma_socket *s = NULL;
    iop_event_t evf_data;
    int i;

    /* Find free socket */
    for (i = 0; i < UDPRDMA_MAX_SOCKETS; i++) {
        if (sockets[i].udp_socket == NULL) {
            s = &sockets[i];
            break;
        }
    }

    if (s == NULL) {
        M_DEBUG("udprdma_create: no free socket\n");
        return NULL;
    }

    /* Initialize socket structure */
    memset(s, 0, sizeof(*s));
    s->port = port ? port : UDPFS_PORT;
    s->service_id = service_id;
    s->state = STATE_INIT;

    /* Detect SPEED chip revision for DMA block size limit */
    {
        USE_SPD_REGS;
        uint16_t spd_rev = SPD_REG16(SPD_R_REV_1);
        s->dma_max_block_size = (spd_rev <= 0x12) ? 128 : 512;
        M_DEBUG("udprdma_create: SPEED rev=0x%02X, max_dma_block=%u\n",
            spd_rev, s->dma_max_block_size);
    }

    /* Bind UDP socket */
    s->udp_socket = udp_bind(s->port, _ist, s);
    if (s->udp_socket == NULL) {
        M_DEBUG("udprdma_create: udp_bind failed\n");
        return NULL;
    }

    /* Create event flag */
    evf_data.attr = 0;
    evf_data.option = 0;
    evf_data.bits = 0;
    s->event_flag = CreateEventFlag(&evf_data);
    if (s->event_flag <= 0) {
        M_DEBUG("udprdma_create: CreateEventFlag failed\n");
        s->udp_socket = NULL;
        return NULL;
    }

    /* Initialize packet headers */
    udp_packet_init((udp_packet_t *)&s->pkt_disc, IP_ADDR(255,255,255,255), s->port);
    udp_packet_init((udp_packet_t *)&s->pkt_data, IP_ADDR(255,255,255,255), s->port);

    M_DEBUG("udprdma_create: socket created on port 0x%04X, service 0x%04X\n",
        s->port, s->service_id);

    return s;
}

void udprdma_destroy(udprdma_socket_t *socket)
{
    if (socket == NULL) return;

    if (socket->event_flag > 0) {
        DeleteEventFlag(socket->event_flag);
    }

    /* Note: UDP socket cleanup not implemented in ministack */
    socket->udp_socket = NULL;
    socket->event_flag = 0;
    socket->state = STATE_INIT;
}

int udprdma_discover(udprdma_socket_t *socket, uint32_t timeout_ms)
{
    iop_sys_clock_t clock;
    uint32_t evf_bits;
    int retries;

    if (socket == NULL) return UDPRDMA_ERR_INVAL;

    if (timeout_ms == 0) timeout_ms = UDPRDMA_DISC_TIMEOUT_US / 1000;

    socket->state = STATE_DISCOVERING;

    for (retries = 0; retries < UDPRDMA_MAX_RETRIES; retries++) {
        /* Send discovery broadcast */
        _send_discovery(socket);

        /* Set timeout */
        USec2SysClock(timeout_ms * 1000 / UDPRDMA_MAX_RETRIES, &clock);
        SetAlarm(&clock, _timeout_cb, socket);

        /* Wait for response or timeout */
        WaitEventFlag(socket->event_flag, EF_RX_INFO | EF_TIMEOUT,
            WEF_OR | WEF_CLEAR, &evf_bits);

        CancelAlarm(_timeout_cb, socket);
        M_DEBUG("udprdma_discover: evf_bits=0x%X\n", evf_bits);

        if (evf_bits & EF_RX_INFO) {
            /* Got INFORM response */
            socket->state = STATE_CONNECTED;
            M_DEBUG("udprdma_discover: connected to %d.%d.%d.%d\n",
                (socket->peer_ip >> 24) & 0xFF,
                (socket->peer_ip >> 16) & 0xFF,
                (socket->peer_ip >>  8) & 0xFF,
                (socket->peer_ip >>  0) & 0xFF);

            /* Update packet destination to peer */
            udp_packet_init((udp_packet_t *)&socket->pkt_disc, socket->peer_ip, socket->port);
            udp_packet_init((udp_packet_t *)&socket->pkt_data, socket->peer_ip, socket->port);

            return UDPRDMA_OK;
        }

        M_DEBUG("udprdma_discover: retry %d\n", retries + 1);
    }

    socket->state = STATE_DISCONNECTED;
    return UDPRDMA_ERR_TIMEOUT;
}

void udprdma_inform(udprdma_socket_t *socket)
{
    if (socket == NULL) return;
    _send_inform(socket);
}

int udprdma_send(udprdma_socket_t *socket, const void *data, uint32_t size)
{
    iop_sys_clock_t clock;
    uint32_t evf_bits;
    uint16_t sent_seq_nr;
    int retries;

    if (socket == NULL || data == NULL) return UDPRDMA_ERR_INVAL;
    if (size == 0) return UDPRDMA_OK;
    if (size > UDPRDMA_MAX_PAYLOAD) size = UDPRDMA_MAX_PAYLOAD;

    if (socket->state != STATE_CONNECTED) {
        return UDPRDMA_ERR_NOTCONN;
    }

    /* Clear stale EF_RX_ACK left over from multi-packet receive */
    ClearEventFlag(socket->event_flag, ~EF_RX_ACK);

    for (retries = 0; retries < UDPRDMA_MAX_RETRIES; retries++) {
        /* Send data and remember sequence number */
        sent_seq_nr = socket->tx_seq_nr;
        _send_data(socket, data, size);

        /* Set timeout */
        USec2SysClock(UDPRDMA_RETX_TIMEOUT_US, &clock);
        SetAlarm(&clock, _timeout_cb, socket);

        /* Wait for ACK or timeout - no WEF_CLEAR to preserve EF_RX_FIN */
        WaitEventFlag(socket->event_flag, EF_RX_ACK | EF_TIMEOUT,
            WEF_OR, &evf_bits);

        CancelAlarm(_timeout_cb, socket);
        ClearEventFlag(socket->event_flag, ~(EF_RX_ACK | EF_TIMEOUT));

        if (evf_bits & EF_RX_ACK) {
            /* Check if our packet was acknowledged */
            /* ACK seq_nr_ack should be >= our sent seq_nr */
            int16_t diff = (int16_t)((socket->tx_seq_nr_acked - sent_seq_nr) & 0xFFF);
            if (diff >= 0 || diff < -2048) {  /* Handle wraparound */
                return UDPRDMA_OK;
            }
            /* ACK was for older packet, continue waiting */
            M_PRINTF("send: stale ACK acked=%d sent=%d, retry %d\n",
                socket->tx_seq_nr_acked, sent_seq_nr, retries + 1);
        }

        if (evf_bits & EF_TIMEOUT) {
            M_PRINTF("send: timeout seq=%d, retry %d\n", sent_seq_nr, retries + 1);
        }

        /* Restore sequence number for retransmit */
        socket->tx_seq_nr = sent_seq_nr;
    }

    /* Max retries exceeded */
    M_PRINTF("send: max retries exceeded, disconnecting\n");
    socket->state = STATE_DISCONNECTED;
    return UDPRDMA_ERR_NACK;
}

int udprdma_send_ll(udprdma_socket_t *socket,
                    const void *app_hdr, uint32_t app_hdr_size,
                    const void *data, uint32_t data_size)
{
    iop_sys_clock_t clock;
    uint32_t evf_bits;
    uint16_t sent_seq_nr;
    int retries;

    if (socket == NULL || app_hdr == NULL || data == NULL) return UDPRDMA_ERR_INVAL;
    if (app_hdr_size > UDPRDMA_MAX_APP_HDR) return UDPRDMA_ERR_INVAL;
    if (app_hdr_size + data_size == 0) return UDPRDMA_OK;

    if (socket->state != STATE_CONNECTED) {
        return UDPRDMA_ERR_NOTCONN;
    }

    /* Clear stale EF_RX_ACK left over from multi-packet receive */
    ClearEventFlag(socket->event_flag, ~EF_RX_ACK);

    for (retries = 0; retries < UDPRDMA_MAX_RETRIES; retries++) {
        sent_seq_nr = socket->tx_seq_nr;
        _send_data_ll(socket, app_hdr, app_hdr_size, data, data_size);

        USec2SysClock(UDPRDMA_RETX_TIMEOUT_US, &clock);
        SetAlarm(&clock, _timeout_cb, socket);

        /* Wait for ACK or timeout - no WEF_CLEAR to preserve EF_RX_FIN */
        WaitEventFlag(socket->event_flag, EF_RX_ACK | EF_TIMEOUT,
            WEF_OR, &evf_bits);

        CancelAlarm(_timeout_cb, socket);
        ClearEventFlag(socket->event_flag, ~(EF_RX_ACK | EF_TIMEOUT));

        if (evf_bits & EF_RX_ACK) {
            int16_t diff = (int16_t)((socket->tx_seq_nr_acked - sent_seq_nr) & 0xFFF);
            if (diff >= 0 || diff < -2048) {
                return UDPRDMA_OK;
            }
            M_PRINTF("send_ll: stale ACK acked=%d sent=%d, retry %d\n",
                socket->tx_seq_nr_acked, sent_seq_nr, retries + 1);
        }

        if (evf_bits & EF_TIMEOUT) {
            M_PRINTF("send_ll: timeout seq=%d, retry %d\n", sent_seq_nr, retries + 1);
        }

        socket->tx_seq_nr = sent_seq_nr;
    }

    M_PRINTF("send_ll: max retries exceeded, disconnecting\n");
    socket->state = STATE_DISCONNECTED;
    return UDPRDMA_ERR_NACK;
}

int udprdma_recv(udprdma_socket_t *socket, void *buffer, uint32_t size, uint32_t timeout_ms)
{
    iop_sys_clock_t clock;
    uint32_t evf_bits;

    if (socket == NULL || buffer == NULL) return UDPRDMA_ERR_INVAL;
    if (size == 0) return 0;

    if (socket->state != STATE_CONNECTED) {
        return UDPRDMA_ERR_NOTCONN;
    }

    if (timeout_ms == 0) timeout_ms = 5000;

    /* Setup receive state (skip if already configured by set_rx_buffer) */
    if (socket->rx_buffer == NULL) {
        socket->rx_buffer = buffer;
        socket->rx_buffer_size = size;
        socket->rx_received = 0;
    }

    /* Wait for FIN/buffer full, NACK (out-of-order), or timeout */
    while (1) {
        USec2SysClock(timeout_ms * 1000, &clock);
        SetAlarm(&clock, _timeout_cb, socket);

        WaitEventFlag(socket->event_flag,
            EF_RX_FIN | EF_RX_NACK | EF_RX_WINDOW | EF_TIMEOUT,
            WEF_OR | WEF_CLEAR, &evf_bits);

        CancelAlarm(_timeout_cb, socket);

        if (evf_bits & EF_RX_FIN) {
            _send_ack(socket, 1);
            int result = socket->rx_received;
            socket->rx_buffer = NULL;
            socket->rx_hdr_buffer = NULL;
            return result;
        }

        if (evf_bits & EF_RX_NACK) {
            _send_ack(socket, 0);
            continue;
        }

        if (evf_bits & EF_RX_WINDOW) {
            _send_ack(socket, 1);
            continue;
        }

        if (evf_bits & EF_TIMEOUT) {
            M_PRINTF("recv: timeout, received=%d/%d\n",
                socket->rx_received, socket->rx_buffer_size);
            socket->rx_buffer = NULL;
            socket->rx_hdr_buffer = NULL;
            socket->state = STATE_DISCONNECTED;
            return UDPRDMA_ERR_TIMEOUT;
        }
    }
}

int udprdma_is_connected(udprdma_socket_t *socket)
{
    if (socket == NULL) return 0;
    return socket->state == STATE_CONNECTED ? 1 : 0;
}

uint32_t udprdma_get_peer_ip(udprdma_socket_t *socket)
{
    if (socket == NULL) return 0;
    return socket->peer_ip;
}

void udprdma_set_rx_buffer(udprdma_socket_t *socket, void *buffer, uint32_t size)
{
    if (socket == NULL) return;
    socket->rx_buffer = buffer;
    socket->rx_buffer_size = size;
    socket->rx_received = 0;
    socket->rx_window_count = 0;
}

void udprdma_set_rx_app_header(udprdma_socket_t *socket, void *hdr_buf, uint32_t hdr_size)
{
    if (socket == NULL) return;
    socket->rx_hdr_buffer = hdr_buf;
    socket->rx_hdr_size = hdr_size;
    socket->rx_hdr_received = 0;
}
