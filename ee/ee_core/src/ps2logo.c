/*
  PS2LOGO region and checksum patcher for neutrino ee_core.

  Patches PS2LOGO (after it has been loaded into EE RAM by SifLoadElf) to:
    1. Always use the disc's region (PAL/NTSC) instead of the console region.

  Based on code from:
  https://github.com/pcm720/OSDMenu
  https://github.com/mlafeldt/ps2logo
*/

#include <kernel.h>
#include <stdint.h>

#include "util.h"

static uint8_t g_isPAL = 0;

// Patches the region getter at the given ROM-version-specific offset.
// getRegionLoc — region getter function, with ROMVER check call at +8 bytes
static int doPatchWithOffsets(uint32_t getRegionLoc) {
    if ((_lw(getRegionLoc) != 0x27bdfff0) || ((_lw(getRegionLoc + 8) & 0xff000000) != 0x0c000000))
        return -1;

    // Patch region getter to always return the disc's region
    _sw((0x24020000 | ((g_isPAL) ? 2 : 0)), getRegionLoc + 8);

    FlushCache(0);
    FlushCache(2);
    return 0;
}

static void doPatch(void) {
    // ROM 1.10
    if (!doPatchWithOffsets(0x100178))
        return;
    // ROM 1.20-1.70
    if (!doPatchWithOffsets(0x102078))
        return;
    // ROM 1.80+
    if (!doPatchWithOffsets(0x102018))
        return;
}

// Wraps ExecPS2 call for ROM 2.00
static void patchedExecPS2(void *entry, void *gp, int argc, char *argv[]) {
    doPatch();
    ExecPS2(entry, gp, argc, argv);
}

// Replaces jump to PS2LOGO entrypoint for ROM 2.20
static void patchedJump(void *entry, void *gp, int argc, char *argv[]) {
    doPatch();
    asm volatile("j 0x100000"); // Jump to PS2LOGO entrypoint
}

// Patches PS2LOGO to use the given region.
// Must be called after SifLoadElf("rom0:PS2LOGO", &elf) returns elf.epc,
// and before CleanExecPS2.
void patchPS2LOGO(uint32_t epc, int isPAL) {
    g_isPAL = isPAL ? 1 : 0;

    if (epc > 0x1000000) { // Packed PS2LOGO (ROM 2.00+)
        if ((_lw(0x1000200) & 0xff000000) == 0x08000000) {
            // ROM 2.20+: replace the jump to 0x100000 in the unpacker
            _sw((0x08000000 | ((uint32_t)patchedJump >> 2)), (uint32_t)0x1000200);
        } else if ((_lw(0x100011c) & 0xff000000) == 0x0c000000) {
            // ROM 2.00: hijack the unpacker's ExecPS2 call
            _sw((0x0c000000 | ((uint32_t)patchedExecPS2 >> 2)), (uint32_t)0x100011c);
        }
        return;
    }
    if (epc > 0x200000)
        return; // Ignore protokernels

    // Unpacked PS2LOGO: patch directly
    doPatch();
}
