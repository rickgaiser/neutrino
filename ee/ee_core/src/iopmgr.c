// libc/newlib
#include <string.h>

// PS2SDK
#include <iopcontrol.h>
#include <loadfile.h>
#include <sifrpc.h>
#include <iopheap.h>
#include <sbv_patches.h>
#include <syscallnr.h>

// Neutrino
#include "ee_debug.h"
#include "iopmgr.h"
#include "asm.h"
#include "util.h"
#include "interface.h"

extern int _iop_reboot_count; // defined in libkernel (iopcontrol.c)

static int set_reg_hook = 0;
static int get_reg_hook = 0;
static int imgdrv_offset = 0;
static void (*Direct_SetSyscall)(s32 syscall_num, void *handler);
static int (*Old_SifSetReg)(u32 register_num, int register_value);
static int (*Old_SifGetReg)(u32 register_num);

// Used by Hook_SifSetDma in asm.S
u32 (*Old_SifSetDma)(SifDmaTransfer_t *sdd, s32 len);

int _SifExecModuleBuffer(const void *ptr, u32 size, u32 arg_len, const char *args, int *mod_res, int dontwait);

//---------------------------------------------------------------------------
void services_start()
{
    DPRINTF("Starting services...\n");
    SifInitRpc(0);
    SifInitIopHeap();
    SifLoadFileInit();
}

//---------------------------------------------------------------------------
void services_exit()
{
    DPRINTF("Exiting services...\n");
    SifExitIopHeap();
    SifLoadFileExit();
    SifExitRpc();
}

//---------------------------------------------------------------------------
// Simple module storage checksum
void module_checksum()
{
    int i, j;
    u32 *pms = (u32 *)eec.ModStorageStart;

    DPRINTF("Module memory checksum:\n");

    for (j = 0; j < EEC_MOD_CHECKSUM_COUNT; j++) {
        u32 ssv = 0;
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
            BGERROR(COLOR_FUNC_IOPREBOOT, 2);
        }
        pms += 1024;
    }
}

#ifdef __EESIO_DEBUG
//---------------------------------------------------------------------------
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

//---------------------------------------------------------------------------
// Reset IOP. This function replaces SifIopReset from the PS2SDK
static int Reset_Iop(const char *arg, int mode)
{
    static SifCmdResetData_t reset_pkt __attribute__((aligned(64)));
    struct t_SifDmaTransfer dmat;
    int arglen;

    _iop_reboot_count++; // increment reboot counter to allow RPC clients to detect unbinding!

    SifStopDma();

    for (arglen = 0; arg[arglen] != '\0'; arglen++)
        reset_pkt.arg[arglen] = arg[arglen];

    reset_pkt.header.psize = sizeof reset_pkt; // dsize is not initialized (and not processed, even on the IOP).
    reset_pkt.header.cid = SIF_CMD_RESET_CMD;
    reset_pkt.arglen = arglen;
    reset_pkt.mode = mode;

    dmat.src = &reset_pkt;
    dmat.dest = (void *)SifGetReg(SIF_SYSREG_SUBADDR);
    dmat.size = sizeof(reset_pkt);
    dmat.attr = SIF_DMA_ERT | SIF_DMA_INT_O;
    SifWriteBackDCache(&reset_pkt, sizeof(reset_pkt));

    DIntr();
    ee_kmode_enter();
    Old_SifSetReg(SIF_REG_SMFLAG, SIF_STAT_BOOTEND);

    if (!Old_SifSetDma(&dmat, 1)) {
        ee_kmode_exit();
        EIntr();
        return 0;
    }

    Old_SifSetReg(SIF_REG_SMFLAG, SIF_STAT_SIFINIT);
    Old_SifSetReg(SIF_REG_SMFLAG, SIF_STAT_CMDINIT);
    Old_SifSetReg(SIF_SYSREG_RPCINIT, 0);
    Old_SifSetReg(SIF_SYSREG_SUBADDR, (int)NULL);
    ee_kmode_exit();
    EIntr();

    return 1;
}

//---------------------------------------------------------------------------
// Reset IOP to include our modules
void New_Reset_Iop(const char *arg, int arglen)
{
    int i;
    void *pIOP_buffer;
    const void *IOPRP_img, *imgdrv_irx, *udnl_irx;
    unsigned int length_rounded, udnl_cmdlen, size_IOPRP_img, size_imgdrv_irx, size_udnl_irx;
    char udnl_mod[10];
    char udnl_cmd[RESET_ARG_MAX + 1];
    irxtab_t *irxtable = (irxtab_t *)eec.ModStorageStart;

    DPRINTF("%s()\n", __FUNCTION__);
#ifdef __EESIO_DEBUG
    print_iop_args(arglen, arg);
#endif

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

//---------------------------------------------------------------------------
// This function is called when SifSetDma catches a reboot request
u32 New_SifSetDma(SifDmaTransfer_t *sdd, s32 len)
{
    struct _iop_reset_pkt *reset_pkt = (struct _iop_reset_pkt *)sdd->src;

    // Validate module storage
    module_checksum();

    // Start services, some games hang here becouse the IOP is not responding
    if (eec.ee_core_flags & EECORE_FLAG_DBC)
        *GS_REG_BGCOLOR = COLOR_LBLUE;

    // Reboot the IOP
    SifInitRpc(0);
    while (!Reset_Iop("", 0)) {}
    while (!SifIopSync()) {}
    services_start();
    sbv_patch_enable_lmb();

    // Reboot the IOP with neutrino modules
    if (eec.ee_core_flags & EECORE_FLAG_DBC)
        *GS_REG_BGCOLOR = COLOR_MAGENTA;
    New_Reset_Iop(NULL, 0);

    // Reboot the IOP with neutrino modules and IOPRP
    if (eec.ee_core_flags & EECORE_FLAG_DBC)
        *GS_REG_BGCOLOR = COLOR_YELLOW;
    New_Reset_Iop(reset_pkt->arg, reset_pkt->arglen);

    // Exit services
    services_exit();

    // Ignore EE still trying to complete the IOP reset
    set_reg_hook = 4;
    get_reg_hook = 1;

    if (eec.ee_core_flags & EECORE_FLAG_DBC)
        *GS_REG_BGCOLOR = COLOR_BLACK;

    return 1;
}

//---------------------------------------------------------------------------
// Function running in kernel mode!
// No printf and keep as simple as possible!
static int Hook_SifSetReg(u32 register_num, int register_value)
{
    if (set_reg_hook == 4 && register_num == SIF_REG_SMFLAG && register_value == SIF_STAT_SIFINIT) {
        set_reg_hook--;
        return 0;
    } else if (set_reg_hook == 3 && register_num == SIF_REG_SMFLAG && register_value == SIF_STAT_CMDINIT) {
        set_reg_hook--;
        return 0;
    } else if (set_reg_hook == 2 && register_num == SIF_SYSREG_RPCINIT && register_value == 0) {
        set_reg_hook--;
        return 0;
    } else if (set_reg_hook == 1 && register_num == SIF_SYSREG_SUBADDR && register_value == (int)NULL) {
        set_reg_hook--;
        if (eec.ee_core_flags & EECORE_FLAG_UNHOOK) {
            //
            // Call kernel functions directly, becouse we are already in kernel mode
            //
            Direct_SetSyscall(__NR_SifSetDma, Old_SifSetDma);
            Direct_SetSyscall(__NR_SifSetReg, Old_SifSetReg);
            Direct_SetSyscall(__NR_SifGetReg, Old_SifGetReg);
        }
        return 0;
    } else if (set_reg_hook == 0 && register_num == SIF_REG_SMFLAG && register_value == SIF_STAT_BOOTEND) {
        // Start of a new reboot sequence
        return 0;
    } else if (set_reg_hook != 0) {
        BGERROR(COLOR_FUNC_IOPREBOOT, 3);
    }

    return Old_SifSetReg(register_num, register_value);
}

//---------------------------------------------------------------------------
// Function running in kernel mode!
// No printf and keep as simple as possible!
static int Hook_SifGetReg(u32 register_num)
{
    if (get_reg_hook == 1 && register_num == SIF_REG_SMFLAG) {
        get_reg_hook--;
        return 0;
    } else if (get_reg_hook != 0) {
        BGERROR(COLOR_FUNC_IOPREBOOT, 4);
    }

    return Old_SifGetReg(register_num);
}

//---------------------------------------------------------------------------
// Replace SifSetDma, SifSetReg and SifGetReg syscalls in kernel
void Install_Kernel_Hooks(void)
{
    Direct_SetSyscall = GetSyscallHandler(__NR_SetSyscall);

    Old_SifSetDma = GetSyscallHandler(__NR_SifSetDma);
    SetSyscall(__NR_SifSetDma, &Hook_SifSetDma);

    Old_SifSetReg = GetSyscallHandler(__NR_SifSetReg);
    SetSyscall(__NR_SifSetReg, &Hook_SifSetReg);

    Old_SifGetReg = GetSyscallHandler(__NR_SifGetReg);
    SetSyscall(__NR_SifGetReg, &Hook_SifGetReg);
}
