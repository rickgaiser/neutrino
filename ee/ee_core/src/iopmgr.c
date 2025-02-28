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
#include "interface.h"

extern int _iop_reboot_count; // defined in libkernel (iopcontrol.c)

int _SifExecModuleBuffer(const void *ptr, u32 size, u32 arg_len, const char *args, int *mod_res, int dontwait);

#ifdef __EESIO_DEBUG
static void print_iop_args(int arg_len, const char *args)
{
    // Multiple null terminated strings together
    int args_idx = 0;
    int was_null = 1;

    if (arg_len == 0)
        return;

    DPRINTF("IOP reboot arguments (arg_len=%d):\n", arg_len);

    // Search strings
    while(args_idx < arg_len) {
        if (args[args_idx] == 0) {
            if (was_null == 1) {
                DPRINTF("- args[%d]=0\n", args_idx);
            }
            was_null = 1;
        }
        else if (was_null == 1) {
            DPRINTF("- args[%d]='%s'\n", args_idx, &args[args_idx]);
            was_null = 0;
        }
        args_idx++;
    }
}
#endif

/*----------------------------------------------------------------*/
/* Reset IOP to include our modules.                              */
/*----------------------------------------------------------------*/
void New_Reset_Iop(const char *arg, int arglen)
{
    int i, j;
    void *pIOP_buffer;
    const void *IOPRP_img, *imgdrv_irx, *udnl_irx;
    unsigned int length_rounded, udnl_cmdlen, size_IOPRP_img, size_imgdrv_irx, size_udnl_irx;
    char udnl_mod[10];
    char udnl_cmd[RESET_ARG_MAX + 1];
    irxtab_t *irxtable = (irxtab_t *)eec.ModStorageStart;
    static int imgdrv_offset = 0;
    static const char *last_arg = (char*)1;

    DPRINTF("%s()\n", __FUNCTION__);
#ifdef __EESIO_DEBUG
    print_iop_args(arglen, arg);
#endif

    // 2x IOP reboot to default state is not needed
    // - 1x from loading into the Emulation Environment
    // - 1x from the game IOP reboot request
    if ((arg == NULL) && (last_arg == NULL)) {
        DPRINTF("%s() - request ignored\n", __FUNCTION__);
        return;
    }
    last_arg = arg;

    //
    // Simple module checksum
    //
    u32 *pms = (u32 *)eec.ModStorageStart;
    DPRINTF("Module memory checksum:\n");
    for (j = 0; j < EEC_MOD_CHECKSUM_COUNT; j++) {
        u32 ssv = 0;
        int i;
        for (i=0; i<1024; i++) {
            ssv += pms[i];
            // Skip imgdrv patch area
            if (pms[i] == 0xDEC1DEC1)
                i += 2;
        }
        if (ssv == eec.mod_checksum_4k[j]) {
            DPRINTF("- 0x%08x = 0x%08x\n", (u32)pms, ssv);
        } else {
            DPRINTF("- 0x%08x = 0x%08x != 0x%08x\n", (u32)pms, ssv, eec.mod_checksum_4k[j]);
            DPRINTF("- FREEZE!\n");
            while (1) {}
        }
        pms += 1024;
    }

    new_iop_reboot_count++;

    udnl_cmdlen = 0;
    if (arglen >= 10) {
        // Copy: rom0:UDNL or rom1:UDNL
        // - Are these the only update modules? Always 9 chars long?
        strncpy(udnl_mod, &arg[0], 10);
        // Make sure it's 0 terminated
        udnl_mod[10-1] = '\0';

        if (arglen > 10) {
            // Copy: arguments
            udnl_cmdlen = arglen-10; // length, including terminating 0
            strncpy(udnl_cmd, &arg[10], udnl_cmdlen);

            // Fix if 0 is not included in length
            if (udnl_cmd[udnl_cmdlen-1] != '\0') {
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
    // 0 = IOPRP.img
    // 1 = imgdrv.irx
    // 2 = udnl.irx
    IOPRP_img       = irxtable->modules[0].ptr;
    size_IOPRP_img  = irxtable->modules[0].size;
    imgdrv_irx      = irxtable->modules[1].ptr;
    size_imgdrv_irx = irxtable->modules[1].size;
    udnl_irx        = irxtable->modules[2].ptr;
    size_udnl_irx   = irxtable->modules[2].size;

    // Manually copy IOPRP to IOP
    length_rounded = (size_IOPRP_img + 0xF) & ~0xF;
    pIOP_buffer = SifAllocIopHeap(length_rounded);
    CopyToIop(IOPRP_img, length_rounded, pIOP_buffer);

    // Patch imgdrv.irx to point to the IOPRP
    for (i = 0; i < size_imgdrv_irx; i += 4) {
        if (*(u32 *)((&((unsigned char *)imgdrv_irx)[i])) == 0xDEC1DEC1) {
            imgdrv_offset = i;
            break;
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

    if (udnl_irx != NULL) {
        // Load custom UDNL
        _SifExecModuleBuffer(udnl_irx, size_udnl_irx, udnl_cmdlen, udnl_cmd, NULL, 1);
    }
    else {
        // Load system UDNL
        _SifLoadModule(udnl_mod, udnl_cmdlen, udnl_cmd, NULL, LF_F_MOD_LOAD, 1);
    }

    DIntr();
    ee_kmode_enter();
    Old_SifSetReg(SIF_REG_SMFLAG, SIF_STAT_SIFINIT);
    Old_SifSetReg(SIF_REG_SMFLAG, SIF_STAT_CMDINIT);
    Old_SifSetReg(SIF_SYSREG_RPCINIT, 0);
    Old_SifSetReg(SIF_SYSREG_SUBADDR, (int)NULL);
    ee_kmode_exit();
    EIntr();

    _iop_reboot_count++; // increment reboot counter to allow RPC clients to detect unbinding!

    while (!SifIopSync()) {
        ;
    }

    services_start();
    // Patch the IOP to support LoadModuleBuffer
    sbv_patch_enable_lmb();

    DPRINTF("Loading extra IOP modules...\n");
    // Skip the first modules:
    // 0 = IOPRP.IMG
    // 1 = imgdrv.irx
    // 2 = udnl.irx
    for (i = 3; i < irxtable->count; i++) {
        irxptr_t p = irxtable->modules[i];
        SifExecModuleBuffer((void *)p.ptr, p.size, p.arg_len, p.args, NULL);
    }

    DPRINTF("New_Reset_Iop complete!\n");

    return;
}
