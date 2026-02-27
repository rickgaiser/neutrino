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

#include "udprdma.h"
#include "ministack_udp.h"
#include "smap.h"
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
 * Interrupt service thread callback
 *
 * Reads packet header from SMAP FIFO via smap_fifo_read and sets event flags.
 * For DATA packets, transfers payload to receive buffer via smap_fifo_read (DMA for large).
 */
static int _ist(udp_socket_t *udp_socket, void *arg, const uint8_t *hdr, uint16_t hdr_len)
{
    struct udprdma_socket *s = (struct udprdma_socket *)arg;
    const udprdma_pkt_disc_t *disc_pkt = (const udprdma_pkt_disc_t *)hdr;
    udprdma_hdr_t base_hdr = disc_pkt->hdr;  /* offset 42-43: within 44-byte pre-read */

    //M_DEBUG("_ist: type=%d seq=%d\n", base_hdr.packet_type, base_hdr.seq_nr);

    switch (base_hdr.packet_type) {
        case UDPRDMA_PT_DISCOVERY: {
            udprdma_hdr_disc_t disc;
            smap_fifo_read(0x2C, &disc, sizeof(udprdma_hdr_disc_t));
            uint16_t service_id = disc.service_id;
            M_DEBUG("_ist: DISCOVERY svc=0x%04X\n", service_id);

            if (service_id == s->service_id) {
                s->peer_ip = IP_ADDR(disc_pkt->ip.addr_src.addr[0],
                                     disc_pkt->ip.addr_src.addr[1],
                                     disc_pkt->ip.addr_src.addr[2],
                                     disc_pkt->ip.addr_src.addr[3]);
                SetEventFlag(s->event_flag, EF_RX_DISC);
            }
            break;
        }

        case UDPRDMA_PT_INFORM: {
            udprdma_hdr_disc_t disc;
            smap_fifo_read(0x2C, &disc, sizeof(udprdma_hdr_disc_t));
            uint16_t service_id = disc.service_id;
            M_DEBUG("_ist: INFORM svc=0x%04X\n", service_id);

            if (service_id == s->service_id) {
                s->peer_ip = IP_ADDR(disc_pkt->ip.addr_src.addr[0],
                                     disc_pkt->ip.addr_src.addr[1],
                                     disc_pkt->ip.addr_src.addr[2],
                                     disc_pkt->ip.addr_src.addr[3]);
                /* Next expected seq is INFORM's seq + 1 */
                s->rx_seq_nr_expected = (base_hdr.seq_nr + 1) & 0xFFF;
                SetEventFlag(s->event_flag, EF_RX_INFO);
            }
            break;
        }

        case UDPRDMA_PT_DATA: {
            udprdma_hdr_data_t data_hdr;
            smap_fifo_read(0x2C, &data_hdr.raw, sizeof(udprdma_hdr_data_t));
            //M_DEBUG("_ist: DATA seq=%d flags=0x%02X bytes=%d\n",
            //    data_hdr.seq_nr_ack, data_hdr.flags, data_hdr.data_byte_count);

            /* Update TX ACK state only from ACK packets */
            if (data_hdr.flags & UDPRDMA_DF_ACK) {
                s->tx_seq_nr_acked = data_hdr.seq_nr_ack;
                SetEventFlag(s->event_flag, EF_RX_ACK);
            }

            /* Handle payload if present */
            {
                uint32_t hdr_size = data_hdr.hdr_word_count * 4;
                uint32_t payload_size = hdr_size + data_hdr.data_byte_count;

                if (payload_size > 0 && s->rx_buffer != NULL) {
                    if (base_hdr.seq_nr == s->rx_seq_nr_expected) {
                        /* Extract app header via smap_fifo_read PIO (first packet only) */
                        if (hdr_size > 0) {
                            if (s->rx_hdr_buffer != NULL && s->rx_hdr_received == 0) {
                                smap_fifo_read(0x30, s->rx_hdr_buffer, hdr_size);
                            } else {
                                /* Skip header bytes — seek past them by doing a dummy read */
                                uint8_t dummy[UDPRDMA_MAX_APP_HDR];
                                smap_fifo_read(0x30, dummy, hdr_size);
                            }
                            s->rx_hdr_received = hdr_size;
                        }

                        /* DMA/PIO data bytes to rx buffer via smap_fifo_read */
                        if (data_hdr.data_byte_count > 0) {
                            uint32_t remaining = s->rx_buffer_size - s->rx_received;
                            uint32_t xfer_size = data_hdr.data_byte_count < remaining ?
                                data_hdr.data_byte_count : remaining;

                            smap_fifo_read(0x30 + hdr_size,
                                (uint8_t *)s->rx_buffer + s->rx_received, xfer_size);

                            s->rx_received += xfer_size;
                        }

                        s->rx_seq_nr_expected = (base_hdr.seq_nr + 1) & 0xFFF;

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
            M_DEBUG("udprdma: unknown packet type %d\n", base_hdr.packet_type);
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
            int16_t diff = (int16_t)((socket->tx_seq_nr_acked - sent_seq_nr) & 0xFFF);
            if (diff >= 0 || diff < -2048) {
                return UDPRDMA_OK;
            }
            M_PRINTF("send: stale ACK acked=%d sent=%d, retry %d\n",
                socket->tx_seq_nr_acked, sent_seq_nr, retries + 1);
        }

        if (evf_bits & EF_TIMEOUT) {
            M_PRINTF("send: timeout seq=%d, retry %d\n", sent_seq_nr, retries + 1);
        }

        /* Restore sequence number for retransmit */
        socket->tx_seq_nr = sent_seq_nr;
    }

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
