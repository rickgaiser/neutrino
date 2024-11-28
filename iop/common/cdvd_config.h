#ifndef __CDVD_CONFIG__
#define __CDVD_CONFIG__

#include <tamtypes.h>

// flags
#define CDVDMAN_COMPAT_FAST_READS (1<<0) // ~MODE_1
#define CDVDMAN_COMPAT_ALT_READ   (1<<1) // MODE_2
#define CDVDMAN_COMPAT_EMU_DVDDL  (1<<2) // MODE_5
#define CDVDMAN_COMPAT_F1_2001    (1<<3)

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
