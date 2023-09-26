// File Handle Interface for BDM
#ifndef FHI_BDM_H
#define FHI_BDM_H


#include <stdint.h>
#include <usbhdfsd-common.h> // bd_fragment_t


#define MODULE_SETTINGS_MAGIC 0xf1f2f3f4
#define BDM_MAX_FILES 6
#define BDM_MAX_FRAGS 64 // 64 * 12bytes = 768bytes

struct fhi_bdm_fragfile
{
    uint8_t frag_start; /// First fragment in the fragment table
    uint8_t frag_count; /// Number of fragments in the fragment table
    uint64_t size;      /// Size of the file in bytes
} __attribute__((packed));

struct fhi_bdm
{
    // Magic number to find
    uint32_t magic;

    uint32_t drvName; /// Driver name: usb, ata, sdc, etc...
    uint32_t devNr;   /// Device number: 0, 1, 2, etc...

    // Fragmented files:
    // 0 = ISO
    struct fhi_bdm_fragfile fragfile[BDM_MAX_FILES];

    // Fragment table, containing the fragments of all files
    bd_fragment_t frags[BDM_MAX_FRAGS];
} __attribute__((packed));


#endif
