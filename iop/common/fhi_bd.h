// File Handle Interface for Block Devices
#ifndef FHI_BD_H
#define FHI_BD_H


#include <stdint.h>
#include <usbhdfsd-common.h> // bd_fragment_t
#include "fhi.h"


#define BDM_MAX_FRAGS 64 // 64 * 12bytes = 768bytes


struct fhi_bd_file
{
    uint8_t frag_start; /// First fragment in the fragment table
    uint8_t frag_count; /// Number of fragments in the fragment table
    uint64_t size;      /// Size of the file in bytes
} __attribute__((packed));

struct fhi_bd
{
    // Magic number to find
    uint32_t magic;

    uint32_t drvName; /// Driver name: usb, ata, sdc, etc...
    uint32_t devNr;   /// Device number: 0, 1, 2, etc...

    // Files:
    // 0 = ISO
    struct fhi_bd_file file[FHI_MAX_FILES];

    // Fragment table, containing the fragments of all files
    bd_fragment_t frags[BDM_MAX_FRAGS];
} __attribute__((packed));


#endif
