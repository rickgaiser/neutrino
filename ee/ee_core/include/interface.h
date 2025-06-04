#ifndef EE_CORE_INTERFACE_H
#define EE_CORE_INTERFACE_H


#include <stdint.h>


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

// Magic number to find the interface struct
#define MODULE_SETTINGS_MAGIC 0xf1f2f3f4

// Compatibility flags
#define EECORE_FLAG_UNHOOK      (1<< 0) // Unhook syscalls, keep value the same as in asm.S!
#define EECORE_FLAG_DBC         (1<< 1) // Debug colors
#define EECORE_FLAG_GSM_FLD_FP  (1<<10) // GSM: Field Mode: Force Progressive
#define EECORE_FLAG_GSM_FRM_FP1 (1<<11) // GSM: Frame Mode: Force Progressive (240p)
#define EECORE_FLAG_GSM_FRM_FP2 (1<<12) // GSM: Frame Mode: Force Progressive (line-double)
#define EECORE_FLAG_GSM_NO_576P (1<<13) // GSM: Disable GSM 576p mode
#define EECORE_FLAG_GSM_C_1     (1<<14) // GSM: Enable FIELD flip type 1
#define EECORE_FLAG_GSM_C_2     (1<<15) // GSM: Enable FIELD flip type 2
#define EECORE_FLAG_GSM_C_3     (1<<16) // GSM: Enable FIELD flip type 3

// Interface to loader
#define EEC_MOD_CHECKSUM_COUNT 32
struct ee_core_data
{
    // Magic number to find
    uint32_t magic;

    uint32_t flags;
    char GameID[16];

    int *CheatList;

    void *ModStorageStart;
    void *ModStorageEnd;
    // Checksums for modules at: 0x95000 - 0xb5000 = 128KiB
    uint32_t mod_checksum_4k[EEC_MOD_CHECKSUM_COUNT];
} __attribute__((packed, aligned(4)));

extern struct ee_core_data eec;


#endif
