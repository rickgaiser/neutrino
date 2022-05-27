#ifndef MINISTACK_H
#define MINISTACK_H


#include <stdint.h>


#define IP_ADDR(a, b, c, d) ((a << 24) | (b << 16) | (c << 8) | d)

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
    uint8_t  addr_dst[6];
    uint8_t  addr_src[6];
    uint16_t type;
} __attribute__((packed)) eth_header_t;

/* IP header (20 bytes) */
typedef struct
{
    uint8_t   hlen;
    uint8_t   tos;
    uint16_t  len;
    uint16_t  id;
    uint8_t   flags;
    uint8_t   frag_offset;
    uint8_t   ttl;
    uint8_t   proto;
    uint16_t  csum;
    ip_addr_t addr_src;
    ip_addr_t addr_dst;
} __attribute__((packed, aligned(2))) ip_header_t; // Aligned 2 for csum calculation

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
} __attribute__((packed, aligned(4))) eth_packet_t;

typedef struct
{
    eth_header_t eth; // 14 bytes
    ip_header_t ip;   // 20 bytes
    char payload[IP_MAX_PAYLOAD];
} __attribute__((packed, aligned(4))) ip_packet_t;

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
} __attribute__((packed, aligned(4))) udp_packet_t;

#define ETH_TYPE_IPV4 0x0800
#define ETH_TYPE_ARP  0x0806

#define IP_PROTOCOL_UDP 0x11

/**
 * Initialize ethernet packet
 * @param pkt Ethernet packet
 * @param type Packet type, like IPv4/ARP/...
 */
void eth_packet_init(eth_packet_t *pkt, uint16_t type);

/**
 * Send ethernet packet
 * @param pkt Ethernet packet
 * @param size Size of the payload in bytes
 * @return 0 on succes, -1 on failure
 */
int eth_packet_send(eth_packet_t *pkt, uint16_t size);

/**
 * Initialize IP packet
 * @param pkt IP packet
 * @param ip_dst Remote IP addres
 */
void ip_packet_init(ip_packet_t *pkt, uint32_t ip_dst);

/**
 * Send IP packet
 * @param pkt IP packet
 * @param size Size of the payload in bytes
 * @return 0 on succes, -1 on failure
 */
int ip_packet_send(ip_packet_t *pkt, uint16_t size);

struct udp_socket;
typedef int (*udp_port_handler)(struct udp_socket *socket, uint16_t pointer, void *arg);
typedef struct udp_socket
{
    uint16_t port_src;
    udp_port_handler handler;
    void *handler_arg;
} udp_socket_t;

/**
 * Bind to UDP port, and start receiving UDP messages
 * @param port_src    UDP port to listen to
 * @param handler     UDP port handler
 * @param handler_arg UDP port handler argument
 * @return pointer to socket, or NULL on error
 */
udp_socket_t *udp_bind(uint16_t port_src, udp_port_handler handler, void *handler_arg);

/**
 * Initialize UDP packet
 * @param pkt UDP packet
 * @param ip_dst Remote IP addres
 * @param port_dst Remote port number
 */
void udp_packet_init(udp_packet_t *pkt, uint32_t ip_dst, uint16_t port_dst);

/**
 * Send UDP packet
 * @param socket UDP socket
 * @param pkt UDP packet
 * @param size Size of the payload in bytes
 * @return 0 on succes, -1 on failure
 */
int udp_packet_send(udp_socket_t *socket, udp_packet_t *pkt, uint16_t size);

/**
 * Set the local IP address
 * @param ip IP addres
 */
void ms_ip_set_ip(uint32_t ip);

/**
 * Get the IP address
 * @return IP addres
 */
uint32_t ms_ip_get_ip();




int arp_add_entry(uint32_t ip, uint8_t mac[6]);
int handle_rx_eth(uint16_t pointer);






#endif
