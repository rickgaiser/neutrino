/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.

  Some parts of the code are taken from HD Project by Polo
*/

#include "ee_core.h"
#include "util.h"
#include "syshook.h"
#include "cheat_api.h"

void *ModStorageStart, *ModStorageEnd;
void *eeloadCopy, *initUserMemory;

int isInit = 0;

// Global data
u32 g_compat_mask = 0;
char GameID[16] = "__UNKNOWN__";
int GameMode = BDM_NOP_MODE;
int EnableDebug = 0;
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

static void set_args_v(const char *arg)
{
    EnableDebug = 1;
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
    g_compat_mask = _strtoui(arg);
    DPRINTF("Compat Mask = 0x%02x\n", g_compat_mask);
}

static int eecoreInit(int argc, char **argv)
{
    int i;

    SifInitRpc(0);

    DINIT();
    DPRINTF("EE core start!\n");

    for (i=0; i<argc; i++) {
        if (!_strncmp(argv[i], "-drv=", 5))
            set_args_drv(&argv[i][5]);
        if (!_strncmp(argv[i], "-v=", 3))
            set_args_v(&argv[i][3]);
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

    /* installing kernel hooks */
    DPRINTF("Installing Kernel Hooks...\n");
    Install_Kernel_Hooks();

    if (EnableDebug)
        GS_BGCOLOUR = 0xff0000; // Blue

    SifExitRpc();

    return i;
}

int main(int argc, char **argv)
{
    if (isInit == 0) {
        // 1st time the ee_core is started (from OPL GUI)

        // Initialize the ee_core
        int argOffset = eecoreInit(argc, argv);
        isInit = 1;

        // Start selected elf file (should be something like "cdrom0:\ABCD_123.45;1")
        LoadExecPS2(argv[argOffset], argc - 1 - argOffset, &argv[1 + argOffset]);
    } else {
        // 2nd time and later the ee_core is started (from LoadExecPS2)
        // LoadExecPS2 is patched so instead of running rom0:EELOAD, this ee_core is started

        // Ignore argv[0], as it contains the name of this module ("EELOAD")
        int i;

        argv++;
        argc--;

        DPRINTF("Starting ELF: %s\n", argv[0]);
        for (i = 0; i < argc; i++) {
            DPRINTF("- argv[%d]=%s\n", i, argv[i]);
        }


        sysLoadElf(argv[0], argc, argv);
    }

    return 0;
}
