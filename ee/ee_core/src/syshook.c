/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.

  Some parts of the code are taken from HD Project by Polo
*/

#include "ee_core.h"
#include "asm.h"
#include "iopmgr.h"
#include "util.h"
#include "patches.h"
#include "syshook.h"

#include <syscallnr.h>
#include <ee_regs.h>
#include <ps2_reg_defs.h>
#include <loadfile.h>

int set_reg_hook = 0;
int get_reg_hook = 0;
int new_iop_reboot_count = 0;

extern void *ModStorageStart, *ModStorageEnd;
extern void *eeloadCopy, *initUserMemory;

// Global data
u32 (*Old_SifSetDma)(SifDmaTransfer_t *sdd, s32 len);
int (*Old_SifSetReg)(u32 register_num, int register_value);
int (*Old_SifGetReg)(u32 register_num);

void services_start()
{
    DPRINTF("Starting services...\n");
    SifInitRpc(0);
    SifInitIopHeap();
    SifLoadFileInit();
}

void services_exit()
{
    DPRINTF("Exiting services...\n");
    SifExitIopHeap();
    SifLoadFileExit();
    SifExitRpc();
}

/*----------------------------------------------------------------------------------------*/
/* This function is called when SifSetDma catches a reboot request.                       */
/*----------------------------------------------------------------------------------------*/
u32 New_SifSetDma(SifDmaTransfer_t *sdd, s32 len)
{
    struct _iop_reset_pkt *reset_pkt = (struct _iop_reset_pkt *)sdd->src;

    // Do IOP reset
    services_start();
    New_Reset_Iop(reset_pkt->arg, reset_pkt->arglen);
    // Exit services
    services_exit();
    // Ignore EE still trying to complete the IOP reset
    set_reg_hook = 4;
    get_reg_hook = 1;

    return 1;
}

static int Hook_SifGetReg(u32 register_num)
{
    if (register_num == SIF_REG_SMFLAG && get_reg_hook > 0) {
        get_reg_hook--;
        return 0;
    }

    return Old_SifGetReg(register_num);
}

/*----------------------------------------------------------------------------------------*/
/* Replace SifSetDma, SifSetReg and SifGetReg syscalls in kernel.                         */
/*----------------------------------------------------------------------------------------*/
void Install_Kernel_Hooks(void)
{
    Old_SifSetDma = GetSyscallHandler(__NR_SifSetDma);
    SetSyscall(__NR_SifSetDma, &Hook_SifSetDma);

    Old_SifSetReg = GetSyscallHandler(__NR_SifSetReg);
    SetSyscall(__NR_SifSetReg, &Hook_SifSetReg);

    Old_SifGetReg = GetSyscallHandler(__NR_SifGetReg);
    SetSyscall(__NR_SifGetReg, &Hook_SifGetReg);
}
