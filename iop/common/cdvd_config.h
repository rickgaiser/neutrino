#ifndef __CDVD_CONFIG__
#define __CDVD_CONFIG__

#include <tamtypes.h>

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
    u8 fs_sectors; // Number of sectors to allocate for sector buffer
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

#endif
