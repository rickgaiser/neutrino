#include "ministack_arp.h"
#include "ministack_ip.h"
#include "smap.h" /* for SMAPGetMACAddress, smap_transmit (via eth_packet_send_ll) */

#include <stdio.h>
#include <thbase.h>  /* for DelayThread */


typedef struct {
    uint8_t  mac[6];
    uint32_t ip;
} arp_entry_t;
#define MS_ARP_ENTRIES 8
arp_entry_t arp_table[MS_ARP_ENTRIES];


int arp_add_entry(uint32_t ip, const uint8_t mac[6])
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
        if (arp_table[i].ip == 0) {
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

int arp_lookup(uint32_t ip, uint8_t mac[6])
{
    int i;
    for (i=0; i<MS_ARP_ENTRIES; i++) {
        if (arp_table[i].ip == ip) {
            mac[0] = arp_table[i].mac[0];
            mac[1] = arp_table[i].mac[1];
            mac[2] = arp_table[i].mac[2];
            mac[3] = arp_table[i].mac[3];
            mac[4] = arp_table[i].mac[4];
            mac[5] = arp_table[i].mac[5];
            return 0;
        }
    }
    return -1;
}

static void arp_send_request(uint32_t ip)
{
    static arp_packet_t req;

    req.eth.addr_dst[0] = 0xff;
    req.eth.addr_dst[1] = 0xff;
    req.eth.addr_dst[2] = 0xff;
    req.eth.addr_dst[3] = 0xff;
    req.eth.addr_dst[4] = 0xff;
    req.eth.addr_dst[5] = 0xff;
    SMAPGetMACAddress(req.eth.addr_src);
    req.eth.type = htons(ETH_TYPE_ARP);
    req.arp.htype = htons(1);
    req.arp.ptype = htons(ETH_TYPE_IPV4);
    req.arp.hlen = 6;
    req.arp.plen = 4;
    req.arp.oper = htons(1); // request
    SMAPGetMACAddress(req.arp.sender_mac);
    req.arp.sender_ip = htonl(ms_ip_get_ip());
    req.arp.target_mac[0] = 0;
    req.arp.target_mac[1] = 0;
    req.arp.target_mac[2] = 0;
    req.arp.target_mac[3] = 0;
    req.arp.target_mac[4] = 0;
    req.arp.target_mac[5] = 0;
    req.arp.target_ip = htonl(ip);
    eth_packet_send_ll((eth_packet_t *)&req, sizeof(arp_header_t), NULL, 0);
}

int arp_resolve(uint32_t ip, uint8_t mac[6])
{
    int retry;

    if (arp_lookup(ip, mac) == 0)
        return 0;

    for (retry=0; retry<5; retry++) {
        arp_send_request(ip);
        DelayThread(100 * 1000); // 100ms
        if (arp_lookup(ip, mac) == 0)
            return 0;
    }
    return -1;
}

int handle_rx_arp(const uint8_t *hdr)
{
    const arp_packet_t *req = (const arp_packet_t *)hdr;
    static arp_packet_t reply;

    // Learn sender's MAC/IP from any ARP packet
    if (ntohl(req->arp.sender_ip) != 0)
        arp_add_entry(ntohl(req->arp.sender_ip), req->arp.sender_mac);

    if (ntohs(req->arp.oper) == 1 && ntohl(req->arp.target_ip) == ms_ip_get_ip()) {
        reply.eth.addr_dst[0] = req->arp.sender_mac[0];
        reply.eth.addr_dst[1] = req->arp.sender_mac[1];
        reply.eth.addr_dst[2] = req->arp.sender_mac[2];
        reply.eth.addr_dst[3] = req->arp.sender_mac[3];
        reply.eth.addr_dst[4] = req->arp.sender_mac[4];
        reply.eth.addr_dst[5] = req->arp.sender_mac[5];
        SMAPGetMACAddress(reply.eth.addr_src);
        reply.eth.type = htons(ETH_TYPE_ARP);
        reply.arp.htype = htons(1); // ethernet
        reply.arp.ptype = htons(ETH_TYPE_IPV4);
        reply.arp.hlen = 6;
        reply.arp.plen = 4;
        reply.arp.oper = htons(2); // reply
        SMAPGetMACAddress(reply.arp.sender_mac);
        reply.arp.sender_ip     = req->arp.target_ip;
        reply.arp.target_mac[0] = req->arp.sender_mac[0];
        reply.arp.target_mac[1] = req->arp.sender_mac[1];
        reply.arp.target_mac[2] = req->arp.sender_mac[2];
        reply.arp.target_mac[3] = req->arp.sender_mac[3];
        reply.arp.target_mac[4] = req->arp.sender_mac[4];
        reply.arp.target_mac[5] = req->arp.sender_mac[5];
        reply.arp.target_ip     = req->arp.sender_ip;
        eth_packet_send_ll((eth_packet_t *)&reply, sizeof(arp_header_t), NULL, 0);
    }

    return -1;
}
