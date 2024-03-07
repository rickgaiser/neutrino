// File Handle Interface for Block Devices
#ifndef FHI_BD_H
#define FHI_BD_H


#include <stdint.h>
#include "fhi.h"


struct fhi_bd
{
    // Magic number to find
    uint32_t magic;

    uint32_t drvName; /// Driver name: usb, ata, sdc, etc...
    uint32_t devNr;   /// Device number: 0, 1, 2, etc...
} __attribute__((packed));


#endif
