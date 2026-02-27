#include "ministack_arp.h"
#include "ministack_ip.h"
#include "smap.h"

#include <stdio.h>


typedef struct {
    uint8_t  mac[6];
    uint32_t ip;
} arp_entry_t;
#define MS_ARP_ENTRIES 8
arp_entry_t arp_table[MS_ARP_ENTRIES];


int arp_add_entry(uint32_t ip, uint8_t mac[6])
{
    int i;

    // Update existing entry
    for (i=0; i<MS_ARP_ENTRIES; i++) {
        if (ip == arp_table[i].ip) {
            arp_table[i].mac[0] = mac[0];
            arp_table[i].mac[1] = mac[1];
            arp_table[i].mac[2] = mac[2];
            arp_table[i].mac[3] = mac[3];
            arp_table[i].mac[4] = mac[4];
            arp_table[i].mac[5] = mac[5];
            return 0;
        }
    }

    // Add new entry
    for (i=0; i<MS_ARP_ENTRIES; i++) {
        if (ip == 0) {
            arp_table[i].ip  = ip;
            arp_table[i].mac[0] = mac[0];
            arp_table[i].mac[1] = mac[1];
            arp_table[i].mac[2] = mac[2];
            arp_table[i].mac[3] = mac[3];
            arp_table[i].mac[4] = mac[4];
            arp_table[i].mac[5] = mac[5];
            return 0;
        }
    }

    return -1;
}

int handle_rx_arp(void)
{
    arp_packet_t req;
    static arp_packet_t reply;
    uint32_t *parp = (uint32_t*)&req;

    /* Read ETH type + ARP header (32 bytes starting at frame offset 12) */
    smap_fifo_read(12, &parp[3], 32);

    if (ntohs(req.arp.oper) == 1 && ntohl(req.arp.target_ip) == ms_ip_get_ip()) {
        reply.eth.addr_dst[0] = req.arp.sender_mac[0];
        reply.eth.addr_dst[1] = req.arp.sender_mac[1];
        reply.eth.addr_dst[2] = req.arp.sender_mac[2];
        reply.eth.addr_dst[3] = req.arp.sender_mac[3];
        reply.eth.addr_dst[4] = req.arp.sender_mac[4];
        reply.eth.addr_dst[5] = req.arp.sender_mac[5];
        SMAPGetMACAddress(reply.eth.addr_src);
        reply.eth.type = htons(ETH_TYPE_ARP);
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
        eth_packet_send_ll((eth_packet_t *)&reply, sizeof(arp_header_t), NULL, 0);
    }

    return -1;
}
