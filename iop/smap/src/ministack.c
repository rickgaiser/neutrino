#include "ministack.h"
#include "main.h"
#include "xfer.h"


void eth_packet_init(eth_packet_t *pkt, u16 type)
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

void ip_packet_init(ip_packet_t *pkt)
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
    pkt->ip.proto            = 0x11;
    //pkt->ip_csum             = ;
    pkt->ip.addr_src.addr[0] = 192; // FIXME: Make static IP configurable
    pkt->ip.addr_src.addr[1] = 168; // FIXME: Make static IP configurable
    pkt->ip.addr_src.addr[2] = 1;   // FIXME: Make static IP configurable
    pkt->ip.addr_src.addr[3] = 10;  // FIXME: Make static IP configurable
    pkt->ip.addr_dst.addr[0] = 255;
    pkt->ip.addr_dst.addr[1] = 255;
    pkt->ip.addr_dst.addr[2] = 255;
    pkt->ip.addr_dst.addr[3] = 255;
}

void udp_packet_init(udp_packet_t *pkt, u16 port)
{
    ip_packet_init((ip_packet_t *)pkt);

    pkt->udp.port_src = IP_PORT(port);
    pkt->udp.port_dst = IP_PORT(port);
    //pkt->udp.len      = ;
    //pkt->udp.csum     = ;
}

static u16 ip_checksum(ip_header_t *ip)
{
    u16 *data = (u16 *)ip;
    int count = 10;
    u32 csum  = 0;

    while (count--)
        csum += *data++;
    csum = (csum >> 16) + (csum & 0xffff);
    csum = (csum >> 16) + (csum & 0xffff);

    return ~((u16)csum & 0xffff);
}

int eth_packet_send(eth_packet_t *pkt, u16 size)
{
    return smap_transmit(pkt, size + sizeof(eth_header_t));
}

int ip_packet_send(ip_packet_t *pkt, u16 size)
{
    pkt->ip.len  = htons(size + sizeof(ip_header_t));
    pkt->ip.csum = 0;
    pkt->ip.csum = ip_checksum(&pkt->ip);

    return eth_packet_send((eth_packet_t *)pkt, size + sizeof(ip_header_t));
}

int udp_packet_send(udp_packet_t *pkt, u16 size)
{
    pkt->udp.len  = htons(size + sizeof(udp_header_t));
    pkt->udp.csum = 0; // not needed

    return ip_packet_send((ip_packet_t *)pkt, size + sizeof(udp_header_t));
}
