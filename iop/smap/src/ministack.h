#ifndef MINISTACK_H
#define MINISTACK_H


#include "tamtypes.h"


/* These automatically convert the address and port to network order.  */
#define IP_ADDR(a, b, c, d) (((d & 0xff) << 24) | ((c & 0xff) << 16) | \
                             ((b & 0xff) << 8) | ((a & 0xff)))
#define IP_PORT(port) (((port & 0xff00) >> 8) | ((port & 0xff) << 8))

static inline u32 htonl(u32 n)
{
    return ((n & 0x000000ff) << 24) |
           ((n & 0x0000ff00) << 8) |
           ((n & 0x00ff0000) >> 8) |
           ((n & 0xff000000) >> 24);
}
#define ntohl htonl

static inline u16 htons(u16 n)
{
    return ((n & 0xff) << 8) | ((n & 0xff00) >> 8);
}
#define ntohs htons

typedef struct
{
    u8 addr[4];
} __attribute__((packed)) ip_addr_t;

/* Ethernet header (14 bytes = 2 byte alignment!) */
typedef struct
{
    u8 addr_dst[6];
    u8 addr_src[6];
    u16 type;
} __attribute__((packed)) eth_header_t;

/* IP header (20 bytes) */
typedef struct
{
    u8 hlen;
    u8 tos;
    u16 len;
    u16 id;
    u8 flags;
    u8 frag_offset;
    u8 ttl;
    u8 proto;
    u16 csum;
    ip_addr_t addr_src;
    ip_addr_t addr_dst;
} __attribute__((packed, aligned(2))) ip_header_t;

/* UDP header (8 bytes) */
typedef struct
{
    u16 port_src;
    u16 port_dst;
    u16 len;
    u16 csum;
} __attribute__((packed)) udp_header_t;

#define ETH_MAX_PAYLOAD 1500
#define IP_MAX_PAYLOAD  1480
#define UDP_MAX_PAYLOAD 1472

typedef struct
{
    eth_header_t eth; // 14 bytes
    char payload[ETH_MAX_PAYLOAD];
} __attribute__((packed, aligned(2))) eth_packet_t;

typedef struct
{
    eth_header_t eth; // 14 bytes
    ip_header_t ip;   // 20 bytes
    char payload[IP_MAX_PAYLOAD];
} __attribute__((packed, aligned(2))) ip_packet_t;

typedef struct
{
    eth_header_t eth; // 14 bytes
    ip_header_t ip;   // 20 bytes
    udp_header_t udp; //  8 bytes
    char payload[UDP_MAX_PAYLOAD];
} __attribute__((packed, aligned(2))) udp_packet_t;

#define ETH_TYPE_IPV4 0x0800

void eth_packet_init(eth_packet_t *pkt, u16 type);
int eth_packet_send(eth_packet_t *pkt, u16 size);

void ip_packet_init(ip_packet_t *pkt);
int ip_packet_send(ip_packet_t *pkt, u16 size);

void udp_packet_init(udp_packet_t *pkt, u16 port);
int udp_packet_send(udp_packet_t *pkt, u16 size);


#endif
