/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.

  Some parts of the code are taken from HD Project by Polo
*/

// PS2SDK
#include <loadfile.h>
#include <sifrpc.h>
#include <iopheap.h>

// Neutrino
#include "ee_debug.h"
#include "iopmgr.h"
#include "patches.h"
#include "util.h"
#include "asm.h"
#include "cheat_api.h"
#include "gsm_api.h"
#include "interface.h"

extern void *_stack_end;

int isInit = 0;
static int callcount = 0;

// Global data
u32 g_ee_core_flags = 0; // easy to use copy for asm.S
struct ee_core_data eec = {MODULE_SETTINGS_MAGIC};

// This function is defined as weak in ps2sdkc, so how
// we are not using time zone, so we can safe some KB
void _ps2sdk_timezone_update() {}

DISABLE_PATCHED_FUNCTIONS();      // Disable the patched functionalities
DISABLE_EXTRA_TIMERS_FUNCTIONS(); // Disable the extra functionalities for timers

int main(int argc, char **argv)
{
    int i;

    isInit = 1;
    callcount++;

    //if (callcount == 1) {
        // 1st time the ee_core is started (from neutrino)

        // Enable debug messages
        // DINIT(); // In PCSX2 I see double messages after this call, why?
    //}

    DPRINTF("EE_CORE main called (argc=%d, callcount=%d):\n", argc, callcount);
    for (i = 0; i < argc; i++) {
        DPRINTF("- argv[%d]=%s\n", i, argv[i]);
    }

    if (callcount == 1) {
        // 1st time the ee_core is started (from neutrino)

        // Easy to use copy for asm.S
        g_ee_core_flags = eec.flags;

        // Enable cheats
        if (eec.CheatList != NULL) {
            DPRINTF("Enabling cheats\n");
            EnableCheats();
        }

        // Enable GSM, only possible when kernel hooks are allowed
        if (((eec.flags & (EECORE_FLAG_GSM_FLD_FP | EECORE_FLAG_GSM_FRM_FP1 | EECORE_FLAG_GSM_FRM_FP2)) != 0) && ((eec.flags & EECORE_FLAG_UNHOOK) == 0)) {
            DPRINTF("Enabling GSM\n");
            EnableGSM();
        }

        // Install kernel hooks
        DPRINTF("Install kernel hooks\n");
        Install_Kernel_Hooks();

        // Start selected elf file (should be something like "cdrom0:\ABCD_123.45;1")
        LoadExecPS2(argv[0], argc - 1, &argv[1]);
    } else {
        // 2nd time and later the ee_core is started (from LoadExecPS2)
        // LoadExecPS2 is patched so instead of running rom0:EELOAD, this ee_core is started

        t_ExecData elf;

        // Ignore argv[0], as it contains the name of this module ("EELOAD")
        argv++;
        argc--;

        // wipe user memory
        WipeUserMemory((void *)&_stack_end, (void *)eec.ModStorageStart);
        // The upper half (from ModStorageEnd to GetMemorySize()) is taken care of by LoadExecPS2().
        // WipeUserMemory((void *)ModStorageEnd, (void *)GetMemorySize());

        if (eec.iop_rm[0] >= 2) {
            // Reboot the IOP into a clean Emulation Environment
            New_Reset_Iop2(NULL, 0, eec.iop_rm[0], 1);
        } else if ((eec.iop_rm[0] == 1) || (callcount <= 2)) {
            // Reboot the IOP into a clean Emulation Environment
            New_Reset_Iop2(NULL, 0, 2, 1);
        }
        // Load the ELF
        services_start();
        int r = SifLoadElf(argv[0], &elf);
        if (!r) {
            apply_patches(argv[0]);

            FlushCache(WRITEBACK_DCACHE);
            FlushCache(INVALIDATE_ICACHE);

            services_exit();
            CleanExecPS2((void *)elf.epc, (void *)elf.gp, argc, argv);
        }

        // Error
        DPRINTF("%s: failed, error code = -%x\n", __FUNCTION__, -r);
        SleepThread();
    }

    return 0;
}
