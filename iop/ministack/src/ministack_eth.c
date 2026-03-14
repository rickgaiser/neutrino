#include "ministack_eth.h"
#include "ministack_arp.h"
#include "ministack_ip.h"
#include "smap.h" /* for smap_transmit */

#include <stdio.h>


int eth_packet_send_ll(eth_packet_t *pkt, uint16_t pktdatasize, const void *data, uint16_t datasize)
{
    return smap_transmit(pkt, sizeof(eth_header_t) + pktdatasize, data, datasize);
}

int handle_rx_eth(uint16_t len, const uint8_t *hdr, uint16_t hdr_len)
{
    const eth_packet_t *pkt = (const eth_packet_t *)hdr;
    uint16_t eth_type = ntohs(pkt->eth.type);

    switch (eth_type) {
        case ETH_TYPE_ARP:
            return handle_rx_arp(hdr);
        case ETH_TYPE_IPV4:
            return handle_rx_ipv4(hdr, hdr_len);
        default:
            //M_DEBUG("ministack: eth: type 0x%X\n", eth_type);
            return -1;
    }
}
