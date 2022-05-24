#ifndef UDPBD_H
#define UDPBD_H


#include "tamtypes.h"
#include "ministack.h"


#define UDPBD_PORT            0xBDBD //The port on which to listen for incoming data
#define UDPBD_HEADER_MAGIC    0xBDBDBDBD
#define UDPBD_CMD_INFO        0x00
#define UDPBD_CMD_READ        0x01
#define UDPBD_CMD_WRITE       0x02
#define UDPBD_MAX_DATA        1408 // 1408 bytes = max 11 x 128b blocks
#define UDPBD_MAX_SECTOR_READ 128

/* UDP BD header (16 bytes) */
typedef struct
{
    u32 magic;
    union
    {
        u32 cmd32;
        struct
        {
            u8 cmd;
            u8 cmdid;
            u8 cmdpkt;
            u8 count;
        };
    };
    u32 par1;
    u32 par2;
} __attribute__((packed, aligned(4))) udpbd_header_t;

#define UDPBD_MAX_PAYLOAD 1454

typedef struct
{
    eth_header_t eth;  // 14 bytes
    ip_header_t ip;    // 20 bytes
    udp_header_t udp;  //  8 bytes
    u16 align;         //  2 bytes - 2byte -> 4byte alignment
    udpbd_header_t bd; // 16 bytes
                       //char payload[UDPBD_MAX_PAYLOAD];
} __attribute__((packed, aligned(4))) udpbd_pkt_t;

int udpbd_init(void);
void udpbd_rx(u16 pointer);

#endif
