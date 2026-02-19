#ifndef MINISTACK_IP_H
#define MINISTACK_IP_H


#include "ministack_eth.h"


#define IP_ADDR(a, b, c, d) (((uint8_t)(a) << 24) | ((uint8_t)(b) << 16) | ((uint8_t)(c) << 8) | (uint8_t)(d))

typedef struct
{
    uint8_t addr[4];
} __attribute__((packed)) ip_addr_t;

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

#define IP_MAX_PAYLOAD (ETH_MAX_PAYLOAD - sizeof(ip_header_t))

typedef struct
{
    eth_header_t eth;   // 14 bytes
    ip_header_t  ip;    // 20 bytes
    uint16_t     align; //  2 bytes - 2byte -> 4byte alignment
    //char payload[IP_MAX_PAYLOAD];
} __attribute__((packed, aligned(4))) ip_packet_t;

#define IP_PROTOCOL_UDP 0x11

/**
 * Initialize IP packet
 * @param pkt IP packet
 * @param ip_dst Remote IP addres
 */
void ip_packet_init(ip_packet_t *pkt, uint32_t ip_dst);

/**
 * Send IP packet low level
 * @param pkt IP packet
 * @param pktdatasize Size of the payload in bytes
 * @param data Separate payload
 * @param datasize Size separate payload in bytes
 * @return 0 on succes, -1 on failure
 */
int ip_packet_send_ll(ip_packet_t *pkt, uint16_t pktdatasize, const void *data, uint16_t datasize);

/**
 * Send IP packet
 * @param header IP packet
 * @param size Size of the payload in bytes
 * @return 0 on succes, -1 on failure
 */
static inline int ip_packet_send(ip_packet_t *header, uint16_t size)
{
    return ip_packet_send_ll(header, size, NULL, 0);
}

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


int handle_rx_ipv4(uint16_t pointer);


#endif
