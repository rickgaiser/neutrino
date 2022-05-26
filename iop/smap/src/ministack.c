#include <smapregs.h>
#include <stdio.h>
#include "ministack.h"
#include "udpbd.h"
#include "xfer.h"


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

void udp_packet_init(udp_packet_t *pkt, uint16_t port)
{
    ip_packet_init((ip_packet_t *)pkt);

    pkt->udp.port_src = htons(port);
    pkt->udp.port_dst = htons(port);
    //pkt->udp.len      = ;
    //pkt->udp.csum     = ;
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

int eth_packet_send(eth_packet_t *pkt, uint16_t size)
{
    return smap_transmit(pkt, size + sizeof(eth_header_t));
}

int ip_packet_send(ip_packet_t *pkt, uint16_t size)
{
    pkt->ip.len  = htons(size + sizeof(ip_header_t));
    pkt->ip.csum = 0;
    pkt->ip.csum = ip_checksum(&pkt->ip);

    return eth_packet_send((eth_packet_t *)pkt, size + sizeof(ip_header_t));
}

int udp_packet_send(udp_packet_t *pkt, uint16_t size)
{
    pkt->udp.len  = htons(size + sizeof(udp_header_t));
    pkt->udp.csum = 0; // not needed

    return ip_packet_send((ip_packet_t *)pkt, size + sizeof(udp_header_t));
}

static inline int handle_rx_arp(uint16_t pointer)
{
    USE_SMAP_REGS;
    arp_packet_t req;
    static arp_packet_t reply;
    uint32_t *parp = (uint32_t*)&req;

    SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = pointer + 12;
    parp[ 3] = SMAP_REG32(SMAP_R_RXFIFO_DATA); //  2
    parp[ 4] = SMAP_REG32(SMAP_R_RXFIFO_DATA); //  6
    parp[ 5] = SMAP_REG32(SMAP_R_RXFIFO_DATA); // 10
    parp[ 6] = SMAP_REG32(SMAP_R_RXFIFO_DATA); // 14
    parp[ 7] = SMAP_REG32(SMAP_R_RXFIFO_DATA); // 18
    parp[ 8] = SMAP_REG32(SMAP_R_RXFIFO_DATA); // 22
    parp[ 9] = SMAP_REG32(SMAP_R_RXFIFO_DATA); // 26
    parp[10] = SMAP_REG32(SMAP_R_RXFIFO_DATA); // 30

    if (ntohs(req.arp.oper) == 1 && req.arp.target_ip == IP_ADDR(192,168,1,10)) {
        reply.eth.addr_dst[0] = req.arp.sender_mac[0];
        reply.eth.addr_dst[1] = req.arp.sender_mac[1];
        reply.eth.addr_dst[2] = req.arp.sender_mac[2];
        reply.eth.addr_dst[3] = req.arp.sender_mac[3];
        reply.eth.addr_dst[4] = req.arp.sender_mac[4];
        reply.eth.addr_dst[5] = req.arp.sender_mac[5];
        SMAPGetMACAddress(reply.eth.addr_src);
        reply.eth.type = htons(0x0806);
        reply.arp.htype = htons(1); // ethernet
        reply.arp.ptype = htons(ETH_TYPE_IPV4);
        reply.arp.hlen = 6;
        reply.arp.plen = 4;
        reply.arp.oper = htons(2); // reply
        SMAPGetMACAddress(reply.arp.sender_mac);
        reply.arp.sender_ip     = req.arp.target_ip;
        reply.arp.target_mac[0] = req.arp.sender_mac[0];
        reply.arp.target_mac[1] = req.arp.sender_mac[1];
        reply.arp.target_mac[2] = req.arp.sender_mac[2];
        reply.arp.target_mac[3] = req.arp.sender_mac[3];
        reply.arp.target_mac[4] = req.arp.sender_mac[4];
        reply.arp.target_mac[5] = req.arp.sender_mac[5];
        reply.arp.target_ip     = req.arp.sender_ip;

        smap_transmit(&reply, 0x2A);
    }


    return -1;
}

static inline int handle_rx_udp(uint16_t pointer)
{
    USE_SMAP_REGS;
    uint16_t dport;

    // Check port
    SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = pointer + 0x24;
    dport = SMAP_REG16(SMAP_R_RXFIFO_DATA);

    switch (dport) {
        case 0xbdbd:
            udpbd_rx(pointer);
            return 0;
        default:
            printf("ministack: udp: dport 0x%X\n", dport);
            return -1;
    }
}

static inline int handle_rx_ipv4(uint16_t pointer)
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
            printf("ministack: ipv4: protocol 0x%X\n", protocol);
            return -1;
    }
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
            printf("ministack: eth: type 0x%X\n", eth_type);
            return -1;
    }
}
