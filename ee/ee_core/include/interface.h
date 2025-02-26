#ifndef EE_CORE_INTERFACE_H
#define EE_CORE_INTERFACE_H


#include <stdint.h>


#define MODULE_SETTINGS_MAGIC 0xf1f2f3f4
#define EEC_MOD_CHECKSUM_COUNT 32

struct ee_core_data
{
    // Magic number to find
    uint32_t magic;

    uint32_t ee_core_flags;
    char GameID[16];
    int GameMode;

    void *initUserMemory;

    int *CheatList;

    void *ModStorageStart;
    void *ModStorageEnd;
    // Checksums for modules at: 0x95000 - 0xb5000 = 128KiB
    uint32_t mod_checksum_4k[EEC_MOD_CHECKSUM_COUNT];
} __attribute__((packed, aligned(4)));

extern struct ee_core_data eec;


#endif
