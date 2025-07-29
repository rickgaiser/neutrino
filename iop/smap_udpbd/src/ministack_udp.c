#include "ministack_udp.h"

#include <smapregs.h>
#include <stdio.h>


void udp_packet_init(udp_packet_t *pkt, uint32_t ip_dst, uint16_t port_dst)
{
    ip_packet_init((ip_packet_t *)pkt, ip_dst);

    //pkt->udp.port_src = ;
    pkt->udp.port_dst = htons(port_dst);
    //pkt->udp.len      = ;
    //pkt->udp.csum     = ;
}

#define UDP_MAX_PORTS 4
static udp_socket_t udp_ports[UDP_MAX_PORTS];
udp_socket_t *udp_bind(uint16_t port_src, udp_port_handler handler, void *handler_arg)
{
    int i;

    for (i=0; i<UDP_MAX_PORTS; i++) {
        if (udp_ports[i].port_src == 0) {
            udp_ports[i].port_src    = port_src;
            udp_ports[i].handler     = handler;
            udp_ports[i].handler_arg = handler_arg;
            return &udp_ports[i];
        }
    }

    return NULL;
}

int udp_packet_send_ll(udp_socket_t *socket, udp_packet_t *pkt, uint16_t pktdatasize, const void *data, uint16_t datasize)
{
    pkt->udp.port_src = socket->port_src;
    pkt->udp.len  = htons(sizeof(udp_header_t) + pktdatasize + datasize);
    pkt->udp.csum = 0; // not needed

    return ip_packet_send_ll((ip_packet_t *)pkt, sizeof(udp_header_t) + pktdatasize, data, datasize);
}

int handle_rx_udp(uint16_t pointer)
{
    USE_SMAP_REGS;
    uint16_t dport;
    int i;

    // Check port
    SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = pointer + 0x24;
    dport = SMAP_REG16(SMAP_R_RXFIFO_DATA);

    for (i=0; i<UDP_MAX_PORTS; i++) {
        if (dport == udp_ports[i].port_src)
            return udp_ports[i].handler(&udp_ports[i], pointer, udp_ports[i].handler_arg);
    }

    //M_DEBUG("ministack: udp: dport 0x%X\n", dport);
    return -1;
}
