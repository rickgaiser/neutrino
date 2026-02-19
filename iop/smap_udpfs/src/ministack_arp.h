#ifndef MINISTACK_ARP_H
#define MINISTACK_ARP_H


#include "ministack_eth.h"


typedef struct
{
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t oper;
    uint8_t  sender_mac[6];
    uint32_t sender_ip;
    uint8_t  target_mac[6];
    uint32_t target_ip;
} __attribute__((packed)) arp_header_t;

typedef struct
{
    eth_header_t eth; // 14 bytes
    arp_header_t arp; // 28 bytes
} __attribute__((packed, aligned(4))) arp_packet_t;


int arp_add_entry(uint32_t ip, uint8_t mac[6]);
int handle_rx_arp(uint16_t pointer);


#endif
