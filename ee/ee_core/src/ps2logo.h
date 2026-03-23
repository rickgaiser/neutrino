#ifndef PS2LOGO_H
#define PS2LOGO_H

#include <stdint.h>

// Patches PS2LOGO to use the given region (isPAL: 0=NTSC, 1=PAL) instead of the
// console region, and removes the logo checksum check. Must be called after
// SifLoadElf("rom0:PS2LOGO") with the epc returned by SifLoadElf, and before CleanExecPS2.
void patchPS2LOGO(uint32_t epc, int isPAL);

#endif
