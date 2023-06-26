#ifndef __CDVD_CONFIG__
#define __CDVD_CONFIG__

#include <tamtypes.h>
#include <usbhdfsd-common.h>

// flags
#define IOPCORE_COMPAT_ALT_READ      0x0001
#define IOPCORE_COMPAT_0_SKIP_VIDEOS 0x0002
#define IOPCORE_COMPAT_EMU_DVDDL     0x0004
#define IOPCORE_COMPAT_ACCU_READS    0x0008
#define IOPCORE_ENABLE_POFF          0x0100

// fakemodule_flags
#define FAKE_MODULE_FLAG_DEV9    (1 << 0) // not used, compiled in
#define FAKE_MODULE_FLAG_USBD    (1 << 1) // Used with BDM-USB or PADEMU
#define FAKE_MODULE_FLAG_SMAP    (1 << 2) // not used, compiled in
#define FAKE_MODULE_FLAG_ATAD    (1 << 3) // not used, compiled in
#define FAKE_MODULE_FLAG_CDVDSTM (1 << 4) // not used, compiled in
#define FAKE_MODULE_FLAG_CDVDFSV (1 << 5) // not used, compiled in

#define ISO_MAX_PARTS 10

struct cdvdman_settings_common
{
    u8 NumParts;
    u8 media;
    u16 flags;
    u32 layer1_start;
    u8 DiscID[5];
    u8 padding[2];
    u8 fakemodule_flags;
} __attribute__((packed));

#define BDM_MAX_FILES 1  // ISO
#define BDM_MAX_FRAGS 64 // 64 * 12bytes = 768bytes

struct cdvdman_fragfile
{
    u8 frag_start; /// First fragment in the fragment table
    u8 frag_count; /// Number of fragments in the fragment table
    u32 size;      /// Size of the file in bytes
} __attribute__((packed));

struct cdvdman_settings_bdm
{
    struct cdvdman_settings_common common;

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
