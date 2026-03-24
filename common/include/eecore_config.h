#ifndef EECORE_CONFIG_H
#define EECORE_CONFIG_H


#include <stdint.h>

#include "module_config.h"

// Pointer to IRX module in hidden EE RAM area
typedef struct
{
    const void *ptr;
    unsigned int size;

    unsigned int arg_len;
    const char *args;
} irxptr_t;

// Table pointing to all IRX modules
typedef struct
{
    irxptr_t *modules;
    int count;
} irxtab_t;

// Compatibility flags
#define EECORE_FLAG_UNHOOK      (1<<0) // Unhook syscalls, keep value the same as in asm.S!
#define EECORE_FLAG_DBC         (1<<1) // Debug colors
#define EECORE_FLAG_GSM_NO_576P (1<<2) // GSM: BIOS < 2.10 = no 576p support
#define EECORE_FLAG_LOGO_PATCH  (1<<3) // PS2LOGO: apply region patch (console region ≠ game region)
#define EECORE_FLAG_LOGO_PAL    (1<<4) // PS2LOGO: force PAL mode (only meaningful if LOGO_PATCH set)

enum EECORE_GSM_VMODE
{
    EECORE_GSM_VMODE_NONE = 0,
    EECORE_GSM_VMODE_FP1, // 240p/288p
    EECORE_GSM_VMODE_FP2, // 480p/576p
    EECORE_GSM_VMODE_1080I_X1, // 1080i x1
    EECORE_GSM_VMODE_1080I_X2, // 1080i x2
    EECORE_GSM_VMODE_1080I_X3, // 1080i x3
};

enum EECORE_GSM_COMP_MODE
{
    EECORE_GSM_COMP_NONE = 0,
    EECORE_GSM_COMP_1, // Compatibility mode 1
    EECORE_GSM_COMP_2, // Compatibility mode 2
    EECORE_GSM_COMP_3, // Compatibility mode 3
};

// Interface to loader
#define EEC_MOD_CHECKSUM_COUNT 32
struct ee_core_data
{
    // Magic number to find
    uint32_t magic;

    uint32_t flags;
    uint32_t iop_rm[3];
    char GameID[12];

    enum EECORE_GSM_VMODE GsmVideoMode;
    enum EECORE_GSM_COMP_MODE GsmCompMode;

    int *CheatList;

    void *ModStorageStart;
    void *ModStorageEnd;
    // Checksums for modules at: 0x95000 - 0xb5000 = 128KiB
    uint32_t mod_checksum_4k[EEC_MOD_CHECKSUM_COUNT];
} __attribute__((packed, aligned(4)));

extern struct ee_core_data eec;


#endif
