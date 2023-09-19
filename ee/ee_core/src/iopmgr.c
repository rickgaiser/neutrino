/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.

  Some parts of the code are taken from HD Project by Polo
*/

#include <iopcontrol.h>
#include <loadfile.h>

#include "ee_core.h"
#include "iopmgr.h"
#include "modules.h"
#include "util.h"
#include "syshook.h"

extern int _iop_reboot_count;
extern void *ModStorageStart;

/*----------------------------------------------------------------*/
/* Reset IOP to include our modules.                              */
/*----------------------------------------------------------------*/
int New_Reset_Iop(const char *arg, int arglen)
{
    int i;
    void *pIOP_buffer;
    const void *IOPRP_img, *imgdrv_irx;
    unsigned int length_rounded, udnl_cmdlen, size_IOPRP_img, size_imgdrv_irx;
    char udnl_mod[10];
    char udnl_cmd[RESET_ARG_MAX + 1];
    irxtab_t *irxtable = (irxtab_t *)ModStorageStart;
    static int imgdrv_offset = 0;

    DPRINTF("New_Reset_Iop start!\n");
    if (EnableDebug)
        GS_BGCOLOUR = 0xFF00FF; // Purple

    iop_reboot_count++;

    SifInitRpc(0);
    SifInitIopHeap();
    SifLoadFileInit();
    sbv_patch_enable_lmb();

    udnl_cmdlen = 0;
    if (arglen > 0) {
        // Copy: rom0:UDNL or rom1:UDNL
        // - Are these the only update modules? Always 9 chars long?
        strncpy(udnl_mod, &arg[0], 10);
        // Make sure it's 0 terminated
        udnl_mod[9] = '\0';

        if (arglen > 10) {
            // Copy: arguments
            udnl_cmdlen = arglen-10; // length, including terminating 0
            strncpy(udnl_cmd, &arg[10], udnl_cmdlen);

            // Fix if 0 is not included
            if (udnl_cmd[udnl_cmdlen-1] != 0) {
                udnl_cmd[udnl_cmdlen] = '\0';
                udnl_cmdlen++;
            }
        }
    } else {
        strncpy(udnl_mod, "rom0:UDNL", 10);
    }

    // Add our own IOPRP image
    strncpy(&udnl_cmd[udnl_cmdlen], "img0:", 6);
    udnl_cmdlen += 6;

    // FIXED modules:
    // 0 = IOPRP image
    // 1 = imgdrv
    IOPRP_img       = irxtable->modules[0].ptr;
    size_IOPRP_img  = irxtable->modules[0].size;
    imgdrv_irx      = irxtable->modules[1].ptr;
    size_imgdrv_irx = irxtable->modules[1].size;

    // Manually copy IOPRP to IOP
    length_rounded = (size_IOPRP_img + 0xF) & ~0xF;
    pIOP_buffer = SifAllocIopHeap(length_rounded);
    CopyToIop(IOPRP_img, length_rounded, pIOP_buffer);

    // Patch imgdrv.irx to point to the IOPRP
    for (i = 0; i < size_imgdrv_irx; i += 4) {
        if (*(u32 *)((&((unsigned char *)imgdrv_irx)[i])) == 0xDEC1DEC1) {
            imgdrv_offset = i;
        }
    }
    *(void **)(UNCACHED_SEG(&((unsigned char *)imgdrv_irx)[imgdrv_offset+4])) = pIOP_buffer;
    *(u32   *)(UNCACHED_SEG(&((unsigned char *)imgdrv_irx)[imgdrv_offset+8])) = size_IOPRP_img;

    // Load patched imgdrv.irx
    SifExecModuleBuffer((void *)imgdrv_irx, size_imgdrv_irx, 0, NULL, NULL);

    // Trigger IOP reboot with update
    DIntr();
    ee_kmode_enter();
    Old_SifSetReg(SIF_REG_SMFLAG, SIF_STAT_BOOTEND);
    ee_kmode_exit();
    EIntr();

    _SifLoadModule(udnl_mod, udnl_cmdlen, udnl_cmd, NULL, LF_F_MOD_LOAD, 1);

    DIntr();
    ee_kmode_enter();
    Old_SifSetReg(SIF_REG_SMFLAG, SIF_STAT_SIFINIT);
    Old_SifSetReg(SIF_REG_SMFLAG, SIF_STAT_CMDINIT);
    Old_SifSetReg(SIF_SYSREG_RPCINIT, 0);
    Old_SifSetReg(SIF_SYSREG_SUBADDR, (int)NULL);
    ee_kmode_exit();
    EIntr();

    SifLoadFileExit(); // OPL's integrated LOADFILE RPC does not automatically unbind itself after IOP resets.

    _iop_reboot_count++; // increment reboot counter to allow RPC clients to detect unbinding!

    while (!SifIopSync()) {
        ;
    }

    SifInitRpc(0);
    SifInitIopHeap();
    SifLoadFileInit();
    sbv_patch_enable_lmb();

    DPRINTF("Loading extra IOP modules...\n");
    // Skip the first 2 modules (IOPRP.IMG and imgdrv.irx)
    for (i = 2; i < irxtable->count; i++) {
        irxptr_t p = irxtable->modules[i];
        SifExecModuleBuffer((void *)p.ptr, p.size, p.arg_len, p.args, NULL);
    }

    if (EnableDebug)
        GS_BGCOLOUR = 0x00FFFF; // Yellow

    DPRINTF("Exiting services...\n");
    SifExitIopHeap();
    SifLoadFileExit();
    SifExitRpc();

    DPRINTF("New_Reset_Iop complete!\n");
    // we have 4 SifSetReg calls to skip in ELF's SifResetIop, not when we use it ourselves
    if (set_reg_disabled)
        set_reg_hook = 4;

    if (EnableDebug)
        GS_BGCOLOUR = 0x000000; // Black

    return 1;
}
