// libc/newlib
#include <string.h>

// Other
#include "compat.h"
#include "../../ee_core/include/interface.h" // EECORE compatibility flags
#include "../../../iop/common/cdvd_config.h" // CDVDMAN compatibility flags


typedef struct
{
    uint32_t flag;
    uint32_t eecore_flags;
    uint32_t cdvdman_flags;
    const char *ioppatch;
} flagcompat_t;

static const flagcompat_t flag_compat[] = {
    {1<<0, 0,                  CDVDMAN_COMPAT_FAST_READ, NULL              }, // MODE 0
    {1<<2, 0,                  CDVDMAN_COMPAT_SYNC_READ, NULL              }, // MODE 2
    {1<<3, EECORE_FLAG_UNHOOK, 0,                        NULL              }, // MODE 3
    {1<<5, 0,                  CDVDMAN_COMPAT_EMU_DVDDL, NULL              }, // MODE 5
    {1<<7, 0,                  0,                        "patch_membo.irx" }, // MODE 7
    {0<<0, 0, 0, NULL},
};

void get_compat_flag(uint32_t flags, uint32_t *eecore, uint32_t *cdvdman, const char **ioppatch)
{
    // Game specific compatibility mode
    const flagcompat_t *p = &flag_compat[0];
    while (p->flag != 0) {
        if ((p->flag & flags) != 0) {
            *eecore  |= p->eecore_flags;  // multiple flags possible
            *cdvdman |= p->cdvdman_flags; // multiple flags possible
            *ioppatch = p->ioppatch;      // only 1 patch possible
        }
        p++;
    }
}

typedef struct
{
    char *game;
    uint32_t eecore_flags;
    uint32_t cdvdman_flags;
    const char *ioppatch;
} gamecompat_t;

static const gamecompat_t game_compat[] = {
    {"SCES_524.12", 0,                    CDVDMAN_COMPAT_SYNC_READ, NULL },              // Jackie Chan Adventures # only needed for USB ?

    // These games write to the EE 0x84000 to 1MiB region, where our EECORE is loaded
    // To figure out if another game needs this mode: start the game in PCSX2 and see if this address is written by the game
    {"SCUS_971.24", EECORE_FLAG_UNHOOK, 0,                       NULL },              // Jak and Daxter - The Precursor Legacy
    {"SCES_503.61", EECORE_FLAG_UNHOOK, 0,                       NULL },              // Jak and Daxter - The Precursor Legacy
    {"SCES_506.14", EECORE_FLAG_UNHOOK, 0,                       NULL },              // Jak and Daxter - The Precursor Legacy
    {"SCPS_150.21", EECORE_FLAG_UNHOOK, 0,                       NULL },              // Jak and Daxter - The Precursor Legacy
    {"SCPS_550.04", EECORE_FLAG_UNHOOK, 0,                       NULL },              // Jak and Daxter - The Precursor Legacy
    {"SCPS_560.03", EECORE_FLAG_UNHOOK, 0,                       NULL },              // Jak and Daxter - The Precursor Legacy
    {"SCUS_972.65", EECORE_FLAG_UNHOOK, 0,                       NULL },              // Jak II
    {"SCES_516.08", EECORE_FLAG_UNHOOK, 0,                       NULL },              // Jak II
    {"SCKA_200.10", EECORE_FLAG_UNHOOK, 0,                       NULL },              // Jak II
    {"SCPS_150.57", EECORE_FLAG_UNHOOK, 0,                       NULL },              // Jak II
    {"SCAJ_200.73", EECORE_FLAG_UNHOOK, 0,                       NULL },              // Jak II
    {"SCUS_973.30", EECORE_FLAG_UNHOOK, 0,                       NULL },              // Jak 3
    {"SCES_524.60", EECORE_FLAG_UNHOOK, 0,                       NULL },              // Jak 3
    {"SCKA_200.40", EECORE_FLAG_UNHOOK, 0,                       NULL },              // Jak 3

    // These games have IOP memory buffer overrun issues
    {"SLES_548.38", 0,                  0,                       "patch_membo.irx" }, // Donkey Xote
    {"SCES_511.76", 0,                  0,                       "patch_membo.irx" }, // Disney's Treasure Planet

    // This game allocates a buffer at a fixed memory location
    // NOTE: only needed for online gaming (I5BOOTN.ELF)
    {"SLUS_973.53", 0,                  0,                       "patch_rc_uya.irx" }, // Ratchet Clank - Up Your Arsenal
    {"SCES_524.56", 0,                  0,                       "patch_rc_uya.irx" }, // Ratchet Clank 3

    // These games send a cdvd break command from EE directly into IOP memory map
    // This causes an interrupt by cdvd, plus a callback that the game needs.
    // In PCSX2 you can see the PCSX2 message "Read Abort" every time this happens.
    //
    // Emulation has so far not been able to reproduce this behaviour,
    // as a workaround extra cdvd callbacks are fired.
    {"SCUS_971.50", 0,                  CDVDMAN_COMPAT_F1_2001,  NULL },              // Formula One 2001 <- not checked
    {"SCES_500.04", 0,                  CDVDMAN_COMPAT_F1_2001,  NULL },              // Formula One 2001 <- checked and working
    {"SCED_502.54", 0,                  CDVDMAN_COMPAT_F1_2001,  NULL },              // Formula One 2001 <- not checked
    {"SCED_503.13", 0,                  CDVDMAN_COMPAT_F1_2001,  NULL },              // Formula One 2001 <- not checked
    {"SCPS_150.19", 0,                  CDVDMAN_COMPAT_F1_2001,  NULL },              // Formula One 2001 <- not checked
    {NULL, 0, 0, NULL},
};

void get_compat_game(const char *id, uint32_t *eecore, uint32_t *cdvdman, const char **ioppatch)
{
    // Game specific compatibility mode
    const gamecompat_t *p = &game_compat[0];
    while (p->game != NULL) {
        if (strcmp(id, p->game) == 0) {
            *eecore  |= p->eecore_flags;  // multiple flags possible
            *cdvdman |= p->cdvdman_flags; // multiple flags possible
            *ioppatch = p->ioppatch;      // only 1 patch possible
            break;
        }
        p++;
    }
}


/****************************************************************************
 * Module storage location
 *
 * For most games it is safe to use the bios memory area between:
 * - 0x084000 - 0x100000 = 496KiB
 *
 * However, some games use a part of this memory area. Use this list to
 * relocate the module storage to another location. Note that ee_core needs
 * per-game changes for this to work also.
 */

typedef struct
{
    char *game;
    void *addr;
} modstorage_t;

static const modstorage_t mod_storage_location_list[] = {
    {"SLUS_209.77", (void *)0x01fc7000}, // Virtua Quest
    {"SLPM_656.32", (void *)0x01fc7000}, // Virtua Fighter Cyber Generation: Judgment Six No Yabou
    {NULL, NULL},                        // Terminator
};

void *get_modstorage(const char *id)
{
    const modstorage_t *p = &mod_storage_location_list[0];
    while (p->game != NULL) {
        if (!strcmp(p->game, id))
            return p->addr;
        p++;
    }
    return NULL;
}
