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

int set_reg_hook;
int set_reg_disabled;
int iop_reboot_count = 0;

extern void *ModStorageStart, *ModStorageEnd;
extern void *eeloadCopy, *initUserMemory;
extern void *_end;

// Global data
u32 (*Old_SifSetDma)(SifDmaTransfer_t *sdd, s32 len);
int (*Old_SifSetReg)(u32 register_num, int register_value);
int (*Old_ExecPS2)(void *entry, void *gp, int num_args, char *args[]);
int (*Old_CreateThread)(ee_thread_t *thread_param);
void (*Old_Exit)(s32 exit_code);

/*----------------------------------------------------------------------------------------*/
/* This function is called when SifSetDma catches a reboot request.                       */
/*----------------------------------------------------------------------------------------*/
u32 New_SifSetDma(SifDmaTransfer_t *sdd, s32 len)
{
    struct _iop_reset_pkt *reset_pkt = (struct _iop_reset_pkt *)sdd->src;

    // does IOP reset
    New_Reset_Iop(reset_pkt->arg, reset_pkt->arglen);

    return 1;
}

// ------------------------------------------------------------------------
void sysLoadElf(char *filename, int argc, char **argv)
{
    int r;
    t_ExecData elf;

    SifInitRpc(0);

    DPRINTF("t_loadElf()\n");

#if 1
    DPRINTF("t_loadElf: Resetting IOP...\n");

    set_reg_disabled = 0;
    New_Reset_Iop(NULL, 0);
    set_reg_disabled = 1;

    iop_reboot_count = 1;

    SifInitRpc(0);
#endif
    SifLoadFileInit();

    DPRINTF("t_loadElf: elf path = '%s'\n", filename);

    if (EnableDebug)
        GS_BGCOLOUR = 0x00ff00; // Green

    DPRINTF("t_loadElf: cleaning user memory...");

    // wipe user memory
    WipeUserMemory((void *)&_end, (void *)ModStorageStart);
    // The upper half (from ModStorageEnd to GetMemorySize()) is taken care of by LoadExecPS2().
    // WipeUserMemory((void *)ModStorageEnd, (void *)GetMemorySize());

    FlushCache(0);

    DPRINTF(" done\n");

    DPRINTF("t_loadElf: loading elf...");
    r = SifLoadElf(filename, &elf);

    if (!r) {
        DPRINTF(" done\n");

        DPRINTF("t_loadElf: trying to apply patches...\n");
        // applying needed patches
        apply_patches(filename);

        FlushCache(0);
        FlushCache(2);

        DPRINTF("t_loadElf: exiting services...\n");
        // exit services
        SifExitIopHeap();
        SifLoadFileExit();
        SifExitRpc();

        DPRINTF("t_loadElf: executing...\n");
        CleanExecPS2((void *)elf.epc, (void *)elf.gp, argc, argv);
    }

    DPRINTF(" failed, error code = -%x\n", -r);

    // Error
    GS_BGCOLOUR = 0xffffff; // White    - shouldn't happen.
    SleepThread();
}

static void unpatchEELOADCopy(void)
{
    vu32 *p = (vu32 *)eeloadCopy;

    p[1] = 0x0240302D; /* daddu    a2, s2, zero */
    p[2] = 0x8FA50014; /* lw       a1, 0x0014(sp) */
    p[3] = 0x8C67000C; /* lw       a3, 0x000C(v1) */
}

static void unpatchInitUserMemory(void)
{
    vu16 *p = (vu16 *)initUserMemory;

    /*
     * Reset the start of user memory to 0x00082000, by changing the immediate value being loaded into $a0.
     *  lui  $a0, 0x0008
     *  jal  InitializeUserMemory
     *  ori  $a0, $a0, 0x2000
     */
    p[0] = 0x0008;
    p[4] = 0x2000;
}

void sysExit(s32 exit_code)
{
    Remove_Kernel_Hooks();
}

/*----------------------------------------------------------------------------------------*/
/* Replace SifSetDma, SifSetReg, Exit syscalls in kernel. (Game Loader)                   */
/*----------------------------------------------------------------------------------------*/
void Install_Kernel_Hooks(void)
{
    Old_SifSetDma = GetSyscallHandler(__NR_SifSetDma);
    SetSyscall(__NR_SifSetDma, &Hook_SifSetDma);

    Old_SifSetReg = GetSyscallHandler(__NR_SifSetReg);
    SetSyscall(__NR_SifSetReg, &Hook_SifSetReg);

    Old_Exit = GetSyscallHandler(__NR_KExit);
    SetSyscall(__NR_KExit, &Hook_Exit);
}

/*----------------------------------------------------------------------------------------*/
/* Restore original SifSetDma, SifSetReg, Exit syscalls in kernel. (Game loader)          */
/*----------------------------------------------------------------------------------------*/
void Remove_Kernel_Hooks(void)
{
    SetSyscall(__NR_SifSetDma, Old_SifSetDma);
    SetSyscall(__NR_SifSetReg, Old_SifSetReg);
    SetSyscall(__NR_KExit, Old_Exit);

    DI();
    ee_kmode_enter();

    unpatchEELOADCopy();
    unpatchInitUserMemory();

    ee_kmode_exit();
    EI();

    FlushCache(0);
    FlushCache(2);
}
