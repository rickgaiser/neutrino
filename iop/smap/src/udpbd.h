#ifndef UDPBD_H
#define UDPBD_H


#include <stdint.h>
#include "ministack.h"


#define UDPBD_PORT            0xBDBD //The port on which to listen for incoming data
#define UDPBD_HEADER_MAGIC    0xBDBDBDBD
#define UDPBD_CMD_INFO        0x00
#define UDPBD_CMD_READ        0x01
#define UDPBD_CMD_WRITE       0x02
#define UDPBD_MAX_DATA        1408 // 1408 bytes = max 11 x 128b blocks
#define UDPBD_MAX_SECTOR_READ 255

/* UDP BD header (16 bytes) */
typedef struct
{
    uint32_t magic;
    union
    {
        uint32_t cmd32;
        struct
        {
            uint8_t cmd;
            uint8_t cmdid;
            uint8_t cmdpkt;
            uint8_t count;
        };
    };
    uint32_t par1;
    uint32_t par2;
} __attribute__((packed, aligned(4))) udpbd_header_t;

#define UDPBD_MAX_PAYLOAD 1454

typedef struct
{
    eth_header_t eth;  // 14 bytes
    ip_header_t ip;    // 20 bytes
    udp_header_t udp;  //  8 bytes
    uint16_t align;    //  2 bytes - 2byte -> 4byte alignment
    udpbd_header_t bd; // 16 bytes
                       //char payload[UDPBD_MAX_PAYLOAD];
} __attribute__((packed, aligned(4))) udpbd_pkt_t;

int udpbd_init(void);

#endif
