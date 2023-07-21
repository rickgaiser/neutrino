/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.

  Some parts of the code are taken from HD Project by Polo
*/

#include <iopcontrol.h>

#include "ee_core.h"
#include "iopmgr.h"
#include "modules.h"
#include "modmgr.h"
#include "util.h"
#include "syshook.h"

extern int _iop_reboot_count;
static int imgdrv_offset_ioprpimg = 0;
static int imgdrv_offset_ioprpsiz = 0;
extern void *ModStorageStart;

static void ResetIopSpecial(const char *args, unsigned int arglen)
{
    int i;
    void *pIOP_buffer, *IOPRP_img, *imgdrv_irx;
    unsigned int length_rounded, CommandLen, size_IOPRP_img, size_imgdrv_irx;
    char command[RESET_ARG_MAX + 1];

    if (arglen > 0) {
        strncpy(command, args, arglen);
        command[arglen] = '\0'; /* In a normal IOP reset process, the IOP reset command line will be NULL-terminated properly somewhere.
                        Since we're now taking things into our own hands, NULL terminate it here.
                        Some games like SOCOM3 will use a command line that isn't NULL terminated, resulting in things like "cdrom0:\RUN\IRX\DNAS300.IMGG;1" */
        _strcpy(&command[arglen + 1], "img0:");
        CommandLen = arglen + 6;
//        _strcpy(&command[arglen + 1], "cdrom0:\\IOPRP1.IMG");
//        CommandLen = arglen + 19;
    } else {
        _strcpy(command, "img0:");
        CommandLen = 5;
//        _strcpy(command, "cdrom0:\\IOPRP1.IMG");
//        CommandLen = 18;
    }

    GetOPLModInfo(OPL_MODULE_ID_IOPRP, &IOPRP_img, &size_IOPRP_img);
    GetOPLModInfo(OPL_MODULE_ID_IMGDRV, &imgdrv_irx, &size_imgdrv_irx);

    length_rounded = (size_IOPRP_img + 0xF) & ~0xF;
    pIOP_buffer = SifAllocIopHeap(length_rounded);

    CopyToIop(IOPRP_img, length_rounded, pIOP_buffer);

    if (imgdrv_offset_ioprpimg == 0 || imgdrv_offset_ioprpsiz == 0) {
        for (i = 0; i < size_imgdrv_irx; i += 4) {
            if (*(u32 *)((&((unsigned char *)imgdrv_irx)[i])) == 0xDEC1DEC1) {
                imgdrv_offset_ioprpimg = i;
            }
            if (*(u32 *)((&((unsigned char *)imgdrv_irx)[i])) == 0xDEC2DEC2) {
                imgdrv_offset_ioprpsiz = i;
            }
        }
    }

    *(void **)(UNCACHED_SEG(&((unsigned char *)imgdrv_irx)[imgdrv_offset_ioprpimg])) = pIOP_buffer;
    *(u32 *)(UNCACHED_SEG(&((unsigned char *)imgdrv_irx)[imgdrv_offset_ioprpsiz])) = size_IOPRP_img;

    LoadMemModule(0, imgdrv_irx, size_imgdrv_irx, 0, NULL);

    DIntr();
    ee_kmode_enter();
    Old_SifSetReg(SIF_REG_SMFLAG, SIF_STAT_BOOTEND);
    ee_kmode_exit();
    EIntr();

    LoadModule("rom0:UDNL", SIF_RPC_M_NOWAIT, CommandLen, command);

    DIntr();
    ee_kmode_enter();
    Old_SifSetReg(SIF_REG_SMFLAG, SIF_STAT_SIFINIT);
    Old_SifSetReg(SIF_REG_SMFLAG, SIF_STAT_CMDINIT);
    Old_SifSetReg(SIF_SYSREG_RPCINIT, 0);
    Old_SifSetReg(SIF_SYSREG_SUBADDR, (int)NULL);
    ee_kmode_exit();
    EIntr();

    LoadFileExit(); // OPL's integrated LOADFILE RPC does not automatically unbind itself after IOP resets.

    _iop_reboot_count++; // increment reboot counter to allow RPC clients to detect unbinding!

    while (!SifIopSync()) {
        ;
    }

    SifInitRpc(0);
    SifInitIopHeap();
    LoadFileInit();
    sbv_patch_enable_lmb();

    DPRINTF("Loading extra IOP modules...\n");
    irxtab_t *irxtable = (irxtab_t *)ModStorageStart;
    // Skip the first 3 modules: IOPRP.IMG, imgdrv.irx and resetspu.irx
    // FIXME: magic number!
    for (i = 3; i < irxtable->count; i++) {
        irxptr_t p = irxtable->modules[i];
        // Modules that emulate the sceCdRead function must operate at a higher
        // priority than the highest possible game priority.
        //
        // If the priority is lower (higher number) then some games will wait in
        // and endless loop for data, but the waiting thread will then cause the
        // data to never be processed.
        //
        // This causes some games to 'need' MODE2 (sync reads) to work.
        switch (GET_OPL_MOD_ID(p.info)) {
            case OPL_MODULE_ID_USBD:
                LoadMemModule(0, p.ptr, GET_OPL_MOD_SIZE(p.info), 10, "thpri=7,8");
                break;
            default:
                LoadMemModule(0, p.ptr, GET_OPL_MOD_SIZE(p.info), 0, NULL);
        }
    }
}

/*----------------------------------------------------------------*/
/* Reset IOP to include our modules.                              */
/*----------------------------------------------------------------*/
int New_Reset_Iop(const char *arg, int arglen)
{
    DPRINTF("New_Reset_Iop start!\n");
    if (EnableDebug)
        GS_BGCOLOUR = 0xFF00FF; // Purple

    iop_reboot_count++;

    // Fast IOP reboot:
    // - 1 IOP reboot to desired state
    SifInitRpc(0);
    SifInitIopHeap();
    LoadFileInit();
    sbv_patch_enable_lmb();

    if (arglen > 0) {
        // Reset with IOPRP image
        ResetIopSpecial(&arg[10], arglen - 10);
    }
    else {
        // Reset without IOPRP image
        ResetIopSpecial(NULL, 0);
    }
    if (EnableDebug)
        GS_BGCOLOUR = 0x00FFFF; // Yellow

    DPRINTF("Exiting services...\n");
    SifExitIopHeap();
    LoadFileExit();
    SifExitRpc();

    DPRINTF("New_Reset_Iop complete!\n");
    // we have 4 SifSetReg calls to skip in ELF's SifResetIop, not when we use it ourselves
    if (set_reg_disabled)
        set_reg_hook = 4;

    if (EnableDebug)
        GS_BGCOLOUR = 0x000000; // Black

    return 1;
}
