/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.

  Some parts of the code are taken from HD Project by Polo
*/

#include "ee_core.h"
#include "ee_core_flag.h"
#include "iopmgr.h"
#include "patches.h"
#include "util.h"
#include "asm.h"
#include "syshook.h"
#include "cheat_api.h"
#include "gsm_api.h"

#include <loadfile.h>

void *ModStorageStart, *ModStorageEnd;
void *eeloadCopy, *initUserMemory;
extern void *_stack_end;

int isInit = 0;

// Global data
u32 g_ee_core_flags = 0;
char GameID[16] = "__UNKNOWN__";
int GameMode = BDM_NOP_MODE;
int *gCheatList = NULL; // Store hooks/codes addr+val pairs

// This function is defined as weak in ps2sdkc, so how
// we are not using time zone, so we can safe some KB
void _ps2sdk_timezone_update() {}

DISABLE_PATCHED_FUNCTIONS();      // Disable the patched functionalities
DISABLE_EXTRA_TIMERS_FUNCTIONS(); // Disable the extra functionalities for timers

static void set_args_drv(const char *arg)
{
    if (!_strncmp(arg, "BDM_ILK_MODE", 12))
        GameMode = BDM_ILK_MODE;
    else if (!_strncmp(arg, "BDM_M4S_MODE", 12))
        GameMode = BDM_M4S_MODE;
    else if (!_strncmp(arg, "BDM_USB_MODE", 12))
        GameMode = BDM_USB_MODE;
    else if (!_strncmp(arg, "BDM_UDP_MODE", 12))
        GameMode = BDM_UDP_MODE;
    else if (!_strncmp(arg, "BDM_ATA_MODE", 12))
        GameMode = BDM_ATA_MODE;
    DPRINTF("Game Mode = %d\n", GameMode);
}

static void set_args_kernel(char *arg)
{
    eeloadCopy = (void *)_strtoui(_strtok(arg, " "));
    initUserMemory = (void *)_strtoui(_strtok(NULL, " "));
}

static void set_args_mod(char *arg)
{
    ModStorageStart = (void *)_strtoui(_strtok(arg, " "));
    ModStorageEnd = (void *)_strtoui(_strtok(NULL, " "));
}

static void set_args_cheat(char *arg)
{
    gCheatList = (void *)_strtoui(_strtok(arg, " "));
}

static void set_args_gameid(const char *arg)
{
    strncpy(GameID, arg, sizeof(GameID) - 1);
    GameID[sizeof(GameID) - 1] = '\0';
}

static void set_args_compat(const char *arg)
{
    // bitmask of the compat. settings
    g_ee_core_flags = _strtoui(arg);
    DPRINTF("Compat Mask = 0x%02x\n", g_ee_core_flags);
}

static int eecoreInit(int argc, char **argv)
{
    int i;

    SifInitRpc(0);

    // DINIT(); // In PCSX2 I see double messages after this call, why?
    DPRINTF("EE core start!\n");

    for (i=0; i<argc; i++) {
        if (!_strncmp(argv[i], "-drv=", 5))
            set_args_drv(&argv[i][5]);
        if (!_strncmp(argv[i], "-ec=", 4))
            set_args_cheat(&argv[i][4]);
        if (!_strncmp(argv[i], "-kernel=", 8))
            set_args_kernel(&argv[i][8]);
        if (!_strncmp(argv[i], "-mod=", 5))
            set_args_mod(&argv[i][5]);
        if (!_strncmp(argv[i], "-gid=", 5))
            set_args_gameid(&argv[i][5]);
        if (!_strncmp(argv[i], "-compat=", 8))
            set_args_compat(&argv[i][8]);
        if (!_strncmp(argv[i], "--b", 3))
            break;
    }
    i++;

    // Enable cheat engine
    if (gCheatList != NULL)
        EnableCheats();

    // Enable GSM, only possible when kernel hooks are allowed
    if (((g_ee_core_flags & (EECORE_FLAG_GSM_FLD_FP | EECORE_FLAG_GSM_FRM_FP1 | EECORE_FLAG_GSM_FRM_FP2)) != 0) && ((g_ee_core_flags & EECORE_FLAG_UNHOOK) == 0))
        EnableGSM();

    /* installing kernel hooks */
    DPRINTF("Installing Kernel Hooks...\n");
    Install_Kernel_Hooks();

    SifExitRpc();

    return i;
}

static int callcount = 0;
int main(int argc, char **argv)
{
    int i;

    callcount++;
    DPRINTF("EE_CORE main called (count=%d):\n", callcount);
    for (i = 0; i < argc; i++) {
        DPRINTF("- argv[%d]=%s\n", i, argv[i]);
    }

    if (isInit == 0) {
        // 1st time the ee_core is started (from neutrino)

        // Initialize the ee_core
        int argOffset = eecoreInit(argc, argv);

        // Reboot the IOP into the Emulation Environment
        services_start();
        New_Reset_Iop(NULL, 0);

        isInit = 1;

        // Start selected elf file (should be something like "cdrom0:\ABCD_123.45;1")
        services_exit();
        LoadExecPS2(argv[argOffset], argc - 1 - argOffset, &argv[1 + argOffset]);
    } else {
        // 2nd time and later the ee_core is started (from LoadExecPS2)
        // LoadExecPS2 is patched so instead of running rom0:EELOAD, this ee_core is started

        t_ExecData elf;

        // Ignore argv[0], as it contains the name of this module ("EELOAD")
        argv++;
        argc--;

        // wipe user memory
        WipeUserMemory((void *)&_stack_end, (void *)ModStorageStart);
        // The upper half (from ModStorageEnd to GetMemorySize()) is taken care of by LoadExecPS2().
        // WipeUserMemory((void *)ModStorageEnd, (void *)GetMemorySize());

        SifInitRpc(0);
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
