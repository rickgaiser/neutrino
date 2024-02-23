// File Handle Interface for Block Devices
#ifndef FHI_BD_H
#define FHI_BD_H


#include <stdint.h>
#include "fhi.h"


struct fhi_bd_info
{
    uint64_t size;    /// Size of the file in bytes
} __attribute__((packed));

struct fhi_bd
{
    // Magic number to find
    uint32_t magic;

    uint32_t drvName; /// Driver name: usb, ata, sdc, etc...
    uint32_t devNr;   /// Device number: 0, 1, 2, etc...

    // Fragmented files:
    // 0 = ISO
    struct fhi_bd_info file[FHI_MAX_FILES];
} __attribute__((packed));


#endif
