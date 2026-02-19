#ifndef UDPRDMA_PACKET_H
#define UDPRDMA_PACKET_H

/*
 * UDPRDMA - Reliable RDMA over UDP for PS2
 *
 * This protocol provides reliable bidirectional data transfer over UDP.
 * It uses Go-Back-N ARQ for reliability with 12-bit sequence numbers.
 *
 * Key features:
 * - Service discovery via DISCOVERY/INFORM packets
 * - Reliable data transfer with ACK/NACK
 * - DMA-optimized with fixed 1408-byte max payload
 * - ACK coalescing (100ms) to reduce overhead
 * - Retransmit on NACK or timeout (500ms)
 *
 * All headers are designed for PS2 RDMA compatibility:
 * - Data must be 4-byte aligned
 * - Header sizes are "(multiple of 4) + 2" bytes
 */

#include <stdint.h>
#include "ministack_udp.h"


/* Packet types */
#define UDPRDMA_PT_DISCOVERY  0  /* Broadcast to find peer */
#define UDPRDMA_PT_INFORM     1  /* Response to discovery */
#define UDPRDMA_PT_DATA       2  /* Data packet with optional payload */

/* Data flags */
#define UDPRDMA_DF_ACK   (1<<0)  /* 1=ACK (seq_nr_ack is last received), 0=NACK (seq_nr_ack is expected) */
#define UDPRDMA_DF_FIN   (1<<1)  /* Final packet of transfer */

/* Service ID - unified protocol (UDPBD is a subset of UDPFS) */
#define UDPRDMA_SVC_UDPFS  0xF5F5

/* Port - unified protocol */
#define UDPFS_PORT    0xF5F6

/* Timing constants (microseconds) */
#define UDPRDMA_ACK_TIMEOUT_US     (100 * 1000)   /* 100ms ACK coalescing */
#define UDPRDMA_RETX_TIMEOUT_US    (500 * 1000)   /* 500ms retransmit timeout */
#define UDPRDMA_DISC_TIMEOUT_US    (2000 * 1000)  /* 2s discovery timeout */

/* Retry limits */
#define UDPRDMA_MAX_RETRIES  4


/*
 * Base header (2 bytes)
 * Present in all UDPRDMA packets.
 */
typedef union {
    uint16_t raw;
    struct {
        uint16_t packet_type : 4;   /* UDPRDMA_PT_* */
        uint16_t seq_nr      : 12;  /* Sequence number (0-4095) */
    };
} __attribute__((packed)) udprdma_hdr_t;

/*
 * Discovery/Inform header (4 bytes after base = 6 bytes total)
 * Used for service discovery.
 */
typedef struct {
    uint16_t service_id;       /* Service identifier */
    uint16_t reserved;         /* Reserved (must be 0) */
} __attribute__((packed)) udprdma_hdr_disc_t;

/*
 * Data header (4 bytes after base = 6 bytes total)
 * Used for reliable data transfer.
 *
 * Payload layout: [app_header: hdr_word_count*4 bytes] [data: data_byte_count bytes]
 * When hdr_word_count == 0, there is no app header (data only).
 * Receiver decides DMA block size based on data_byte_count alignment.
 */
typedef union {
    uint32_t raw;
    struct {
        uint32_t seq_nr_ack      : 12;  /* ACK: last received seq_nr, NACK: expected seq_nr */
        uint32_t flags           : 2;   /* UDPRDMA_DF_* */
        uint32_t hdr_word_count  : 4;   /* App header size in 4-byte words (0-60 bytes) */
        uint32_t data_byte_count : 14;  /* Data payload size in bytes (0 = ACK/NACK only) */
    };
} __attribute__((packed)) udprdma_hdr_data_t;

/* Calculate sizes from data header */
#define UDPRDMA_HDR_SIZE(hdr)     ((hdr).hdr_word_count * 4)
#define UDPRDMA_DATA_SIZE(hdr)    ((hdr).data_byte_count)
#define UDPRDMA_PAYLOAD_SIZE(hdr) (UDPRDMA_HDR_SIZE(hdr) + UDPRDMA_DATA_SIZE(hdr))

/* Maximum data payload: 1408 bytes = 11 * 128, fits in standard 1500 MTU */
#define UDPRDMA_MAX_PAYLOAD  1408

/* Maximum app header for scatter-gather send */
#define UDPRDMA_MAX_APP_HDR  32


/*
 * Complete packet structures
 */

/* Discovery packet */
typedef struct {
    eth_header_t      eth;   /* 14 bytes */
    ip_header_t       ip;    /* 20 bytes */
    udp_header_t      udp;   /*  8 bytes */
    udprdma_hdr_t     hdr;   /*  2 bytes */
    udprdma_hdr_disc_t disc; /*  4 bytes */
} __attribute__((packed, aligned(4))) udprdma_pkt_disc_t;

/* Inform packet (same structure as discovery) */
typedef udprdma_pkt_disc_t udprdma_pkt_info_t;

/* Data packet (header only, payload follows) */
typedef struct {
    eth_header_t      eth;   /* 14 bytes */
    ip_header_t       ip;    /* 20 bytes */
    udp_header_t      udp;   /*  8 bytes */
    udprdma_hdr_t     hdr;   /*  2 bytes */
    udprdma_hdr_data_t data; /*  4 bytes */
    uint8_t           extra[UDPRDMA_MAX_APP_HDR]; /* App-level header for scatter-gather send */
} __attribute__((packed, aligned(4))) udprdma_pkt_data_t;


#endif /* UDPRDMA_PACKET_H */
