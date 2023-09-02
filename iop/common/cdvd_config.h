#ifndef __CDVD_CONFIG__
#define __CDVD_CONFIG__

#include <tamtypes.h>
#include <usbhdfsd-common.h> // bd_fragment_t

// flags
#define IOPCORE_COMPAT_ACCU_READS    0x0008 // MODE_1
#define IOPCORE_COMPAT_ALT_READ      0x0001 // MODE_2
//#define IOPCORE_COMPAT_0_SKIP_VIDEOS 0x0002 // MODE_4 - not supported!
#define IOPCORE_COMPAT_EMU_DVDDL     0x0004 // MODE_5
//#define IOPCORE_ENABLE_POFF          0x0100 // MODE_6 - not supported!

#define MODULE_SETTINGS_MAGIC 0xf1f2f3f4
struct cdvdman_settings_common
{
    // Magic number to find
    u32 magic;

    u8 media;
    u16 flags;
    u32 layer1_start;

    union {
        u8 ilink_id[8];
        u64 ilink_id_int;
    };

    union {
        u8 disk_id[5];
        u64 disk_id_int; // 8 bytes, but that's ok for compare reasons
    };
} __attribute__((packed));

#define BDM_MAX_FILES 1  // ISO
#define BDM_MAX_FRAGS 64 // 64 * 12bytes = 768bytes

struct cdvdman_fragfile
{
    u8 frag_start; /// First fragment in the fragment table
    u8 frag_count; /// Number of fragments in the fragment table
    u64 size;      /// Size of the file in bytes
} __attribute__((packed));

struct cdvdman_settings_bdm
{
    struct cdvdman_settings_common common;

    u32 drvName; /// Driver name: usb, ata, sdc, etc...
    u32 devNr;   /// Device number: 0, 1, 2, etc...

    // Fragmented files:
    // 0 = ISO
    struct cdvdman_fragfile fragfile[BDM_MAX_FILES];

    // Fragment table, containing the fragments of all files
    bd_fragment_t frags[BDM_MAX_FRAGS];
} __attribute__((packed));

struct cdvdman_settings_file
{
    struct cdvdman_settings_common common;

    char filename[256];
} __attribute__((packed));

#endif
