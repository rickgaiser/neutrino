#include "ministack_ip.h"
#include "ministack_udp.h"

#include <smapregs.h>
#include <stdio.h>


uint32_t ip_addr = IP_ADDR(192, 168, 1, 10);


void ip_packet_init(ip_packet_t *pkt, uint32_t ip_dest)
{
    eth_packet_init((eth_packet_t *)pkt, ETH_TYPE_IPV4);

    // IP, broadcast
    pkt->ip.hlen             = 0x45;
    pkt->ip.tos              = 0;
    //pkt->ip_len              = ;
    pkt->ip.id               = 0;
    pkt->ip.flags            = 0;
    pkt->ip.frag_offset      = 0;
    pkt->ip.ttl              = 64;
    pkt->ip.proto            = IP_PROTOCOL_UDP;
    //pkt->ip_csum             = ;
    pkt->ip.addr_src.addr[0] = (ip_addr >> 24) & 0xff;
    pkt->ip.addr_src.addr[1] = (ip_addr >> 16) & 0xff;
    pkt->ip.addr_src.addr[2] = (ip_addr >>  8) & 0xff;
    pkt->ip.addr_src.addr[3] = (ip_addr      ) & 0xff;
    pkt->ip.addr_dst.addr[0] = (ip_dest >> 24) & 0xff;
    pkt->ip.addr_dst.addr[1] = (ip_dest >> 16) & 0xff;
    pkt->ip.addr_dst.addr[2] = (ip_dest >>  8) & 0xff;
    pkt->ip.addr_dst.addr[3] = (ip_dest      ) & 0xff;
}

static uint16_t ip_checksum(ip_header_t *ip)
{
    uint16_t *data = (uint16_t *)ip;
    int count = 10;
    uint32_t csum  = 0;

    while (count--)
        csum += *data++;
    csum = (csum >> 16) + (csum & 0xffff);
    csum = (csum >> 16) + (csum & 0xffff);

    return ~((uint16_t)csum & 0xffff);
}

int ip_packet_send_ll(ip_packet_t *pkt, uint16_t pktdatasize, const void *data, uint16_t datasize)
{
    pkt->ip.len  = htons(sizeof(ip_header_t) + pktdatasize + datasize);
    pkt->ip.csum = 0;
    pkt->ip.csum = ip_checksum(&pkt->ip);

    return eth_packet_send_ll((eth_packet_t *)pkt, sizeof(ip_header_t) + pktdatasize, data, datasize);
}

int handle_rx_ipv4(uint16_t pointer)
{
    USE_SMAP_REGS;
    uint8_t protocol;

    // Check ethernet type
    SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = pointer + 0x14;
    protocol = SMAP_REG32(SMAP_R_RXFIFO_DATA) >> 24;

    switch (protocol) {
        case IP_PROTOCOL_UDP:
            return handle_rx_udp(pointer);
        default:
            //M_DEBUG("ministack: ipv4: protocol 0x%X\n", protocol);
            return -1;
    }
}

void ms_ip_set_ip(uint32_t ip)
{
    ip_addr = ip;
}

uint32_t ms_ip_get_ip()
{
    return ip_addr;
}
