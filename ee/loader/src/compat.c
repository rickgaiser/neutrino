// libc/newlib
#include <string.h>

// Other
#include "compat.h"
#include "ee_core.h"


/****************************************************************************
 * Compatibility options:
 * #define COMPAT_MODE_1 0x01 // Accurate reads (sceCdRead)
 * #define COMPAT_MODE_2 0x02 // Sync reads (sceCdRead)
 * #define COMPAT_MODE_3 0x04 // Unhook syscalls
 * #define COMPAT_MODE_4 0x08 // Skip videos - not supported!
 * #define COMPAT_MODE_5 0x10 // Emulate DVD-DL
 * #define COMPAT_MODE_6 0x20 // Disable IGR - not supported!
 */


typedef struct
{
    char *game;
    u32 flags;
} gamecompat_t;

static const gamecompat_t default_game_compat[] = {
    {"SCES_524.12", COMPAT_MODE_2              }, // Jackie Chan Adventures (APEMOD)
    {"SLES_541.61", COMPAT_MODE_2              }, // Super Dragon Ball Z - untested
    {"SLUS_214.42", COMPAT_MODE_2              }, // Super Dragon Ball Z - untested
    {"SLES_548.35", COMPAT_MODE_2              }, // Power Rangers - Super Legends (IOPRP300) <-- not needed on OPL ?
    {"SLUS_216.79", COMPAT_MODE_2              }, // Power Rangers - Super Legends (IOPRP300) <-- not needed on OPL ?
    {NULL, 0},
};

/*
   Games NOT working, even WITH compatibility modes:
    {"SCES_524.60", COMPAT_MODE_2|COMPAT_MODE_3}, // Jak 3 (IOPRP271)
    {"SLUS_212.87", COMPAT_MODE_2              }, // Prince of Persia Two Thrones (IOPRP300)

   Games working without compatibility modes:
    {"SCES_555.73", 0                          }, // MotorStorm Arctic Edge (IOPRP310)
    {"SLES_550.02", 0                          }, // Need for Speed Pro Street (IOPRP280)
    {"SCES_516.07", 0                          }, // Ratchet Clank - Going Commando (IOPRP255)
    {"SCUS_973.53", 0                          }, // Ratchet Clank - Up Your Arsenal (IOPRP300)
    {"SCUS_971.13", 0                          }, // ICO (CD)
    {"SLES_549.45", 0                          }, // DragonBall Z Budokai Tenkaichi 3 (IOPRP300)
    {"SLES_501.26", 0                          }, // Quake III Revolution (CD, IOPRP205)
 */

u32 get_compat(const char *id)
{
    const gamecompat_t *p = &default_game_compat[0];
    while (p->game != NULL) {
        if (strcmp(id, p->game) == 0)
            return p->flags;
        p++;
    }
    return 0;
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
