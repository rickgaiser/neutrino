#ifndef MINISTACK_ETH_H
#define MINISTACK_ETH_H


#include <stdint.h>
#include <stddef.h>


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

/* Ethernet header (14 bytes = 2 byte alignment!) */
typedef struct
{
    uint8_t  addr_dst[6];
    uint8_t  addr_src[6];
    uint16_t type;
} __attribute__((packed)) eth_header_t;

#define ETH_MAX_PAYLOAD 1500

typedef struct
{
    eth_header_t eth;   // 14 bytes
    uint16_t     align; //  2 bytes - 2byte -> 4byte alignment
    //char payload[ETH_MAX_PAYLOAD];
} __attribute__((packed, aligned(4))) eth_packet_t;

#define ETH_TYPE_IPV4 0x0800
#define ETH_TYPE_ARP  0x0806

/**
 * Initialize ethernet packet
 * @param pkt Ethernet packet
 * @param type Packet type, like IPv4/ARP/...
 */
void eth_packet_init(eth_packet_t *pkt, uint16_t type);

/**
 * Send ethernet packet low level
 * @param pkt Ethernet packet
 * @param pktdatasize Size of the payload in bytes
 * @param data Separate payload
 * @param datasize Size separate payload in bytes
 * @return 0 on succes, -1 on failure
 */
int eth_packet_send_ll(eth_packet_t *pkt, uint16_t pktdatasize, const void *data, uint16_t datasize);

/**
 * Send ethernet packet
 * @param pkt Ethernet packet
 * @param size Size of the payload in bytes
 * @return 0 on succes, -1 on failure
 */
static inline int eth_packet_send(eth_packet_t *pkt, uint16_t size)
{
    return eth_packet_send_ll(pkt, size, NULL, 0);
}

int handle_rx_eth(uint16_t pointer);


#endif
