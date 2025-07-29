#include "ministack_eth.h"
#include "ministack_arp.h"
#include "ministack_ip.h"
#include "xfer.h"

#include <smapregs.h>
#include <stdio.h>


void eth_packet_init(eth_packet_t *pkt, uint16_t type)
{
    // Ethernet, broadcast
    pkt->eth.addr_dst[0] = 0xff;
    pkt->eth.addr_dst[1] = 0xff;
    pkt->eth.addr_dst[2] = 0xff;
    pkt->eth.addr_dst[3] = 0xff;
    pkt->eth.addr_dst[4] = 0xff;
    pkt->eth.addr_dst[5] = 0xff;
    SMAPGetMACAddress(pkt->eth.addr_src);
    pkt->eth.type = htons(type);
}

int eth_packet_send_ll(eth_packet_t *pkt, uint16_t pktdatasize, const void *data, uint16_t datasize)
{
    return smap_transmit(pkt, sizeof(eth_header_t) + pktdatasize, data, datasize);
}

int handle_rx_eth(uint16_t pointer)
{
    USE_SMAP_REGS;
    uint16_t eth_type;

    // Check ethernet type
    SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = pointer + 12;
    eth_type = ntohs(SMAP_REG16(SMAP_R_RXFIFO_DATA));

    switch (eth_type) {
        case ETH_TYPE_ARP:
            return handle_rx_arp(pointer);
        case ETH_TYPE_IPV4:
            return handle_rx_ipv4(pointer);
        default:
            //M_DEBUG("ministack: eth: type 0x%X\n", eth_type);
            return -1;
    }
}
