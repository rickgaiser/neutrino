#ifndef MINISTACK_UDP_H
#define MINISTACK_UDP_H


#include "ministack_ip.h"


/* UDP header (8 bytes) */
typedef struct
{
    uint16_t port_src;
    uint16_t port_dst;
    uint16_t len;
    uint16_t csum;
} __attribute__((packed)) udp_header_t;

#define UDP_MAX_PAYLOAD (IP_MAX_PAYLOAD - sizeof(udp_header_t))

typedef struct
{
    eth_header_t eth;   // 14 bytes
    ip_header_t  ip;    // 20 bytes
    udp_header_t udp;   //  8 bytes
    uint16_t     align; //  2 bytes - 2byte -> 4byte alignment
    //char payload[UDP_MAX_PAYLOAD];
} __attribute__((packed, aligned(4))) udp_packet_t;

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
 * Send UDP packet low level
 * @param socket UDP socket
 * @param pkt UDP packet
 * @param pktdatasize Size of the payload in bytes
 * @param data Separate payload
 * @param datasize Size separate payload in bytes
 * @return 0 on succes, -1 on failure
 */
int udp_packet_send_ll(udp_socket_t *socket, udp_packet_t *pkt, uint16_t pktdatasize, const void *data, uint16_t datasize);

/**
 * Send UDP packet
 * @param socket UDP socket
 * @param pkt UDP packet
 * @param size Size of the payload in bytes
 * @return 0 on succes, -1 on failure
 */
static inline int udp_packet_send(udp_socket_t *socket, udp_packet_t *pkt, uint16_t size)
{
    return udp_packet_send_ll(socket, pkt, size, NULL, 0);
}


int handle_rx_udp(uint16_t pointer);


#endif
