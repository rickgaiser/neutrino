#ifndef MINISTACK_H
#define MINISTACK_H


#include <stdint.h>


/* These automatically convert the address and port to network order.  */
#define IP_ADDR(a, b, c, d) (((d & 0xff) << 24) | ((c & 0xff) << 16) | \
                             ((b & 0xff) << 8) | ((a & 0xff)))

static inline uint32_t htonl(uint32_t n)
{
    return ((n & 0x000000ff) << 24) |
           ((n & 0x0000ff00) << 8) |
           ((n & 0x00ff0000) >> 8) |
           ((n & 0xff000000) >> 24);
}
#define ntohl htonl

static inline uint16_t htons(uint16_t n)
{
    return ((n & 0xff) << 8) | ((n & 0xff00) >> 8);
}
#define ntohs htons

typedef struct
{
    uint8_t addr[4];
} __attribute__((packed)) ip_addr_t;

/* Ethernet header (14 bytes = 2 byte alignment!) */
typedef struct
{
    uint8_t addr_dst[6];
    uint8_t addr_src[6];
    uint16_t type;
} __attribute__((packed)) eth_header_t;

/* IP header (20 bytes) */
typedef struct
{
    uint8_t  hlen;
    uint8_t  tos;
    uint16_t len;
    uint16_t id;
    uint8_t  flags;
    uint8_t  frag_offset;
    uint8_t  ttl;
    uint8_t  proto;
    uint16_t csum;
    ip_addr_t addr_src;
    ip_addr_t addr_dst;
} __attribute__((packed, aligned(2))) ip_header_t;

typedef struct
{
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t oper;
    uint8_t  sender_mac[6];
    uint32_t sender_ip;
    uint8_t  target_mac[6];
    uint32_t target_ip;
    uint16_t padding;
} __attribute__((packed)) arp_header_t;

/* UDP header (8 bytes) */
typedef struct
{
    uint16_t port_src;
    uint16_t port_dst;
    uint16_t len;
    uint16_t csum;
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
    arp_header_t arp;
} __attribute__((packed, aligned(4))) arp_packet_t;

typedef struct
{
    eth_header_t eth; // 14 bytes
    ip_header_t ip;   // 20 bytes
    udp_header_t udp; //  8 bytes
    char payload[UDP_MAX_PAYLOAD];
} __attribute__((packed, aligned(2))) udp_packet_t;

#define ETH_TYPE_IPV4 0x0800
#define ETH_TYPE_ARP  0x0806

#define IP_PROTOCOL_UDP 0x11

void eth_packet_init(eth_packet_t *pkt, uint16_t type);
int eth_packet_send(eth_packet_t *pkt, uint16_t size);

void ip_packet_init(ip_packet_t *pkt, uint32_t ip);
int ip_packet_send(ip_packet_t *pkt, uint16_t size);

void udp_packet_init(udp_packet_t *pkt, uint32_t ip, uint16_t port);
int udp_packet_send(udp_packet_t *pkt, uint16_t size);

typedef int (*udp_port_handler)(uint16_t pointer, void *arg);
void udp_bind_port(uint16_t port, udp_port_handler isr, void *isr_arg);
int arp_add_entry(uint32_t ip, uint8_t mac[6]);

int handle_rx_eth(uint16_t pointer);

// IP settings
// NOTE: only local networks, so no subnet mask or gateway!
void ms_ip_set_ip(uint32_t ip);
uint32_t ms_ip_get_ip();


#endif
