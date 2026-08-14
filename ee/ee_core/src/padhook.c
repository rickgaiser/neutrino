/*
  padhook.c - In-Game Reset (IGR) for Neutrino

  Ported from Open-PS2-Loader's ee_core padhook (GPL / AFL 3.0):
    Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
    Copyright 2006-2008 Polo
  PadOpen hooking inspired by ps2rd (jimmikaelkael, misfire).

  Reset-only Neutrino port. The combo L1+L2+R1+R2+SELECT+START returns to the
  configured launcher. Front-panel interception is an independent option.
*/

#include <kernel.h>
#include <loadfile.h>
#include <iopheap.h>
#include <sbv_patches.h>
#include <sifrpc.h>
#include <iopcontrol.h>
#include <string.h>

#include "asm.h"
#include "cheat_api.h"
#include "ee_debug.h"
#include "eecore_config.h"
#include "gsm_api.h"
#include "iopmgr.h"
#include "util.h"
#include "padhook.h"
#include "padpatterns.h"
#include "tlb.h"

// EE DMA / GS hardware registers used by the reset sequence.
// Defined locally because Neutrino's ee_core ships its own ee_regs.h
// (a register-save struct) that shadows ps2sdk's hardware-register header.
#define IGR_R_D_CTRL    ((volatile u32 *)0x1000e000)
#define IGR_R_D_STAT    ((volatile u32 *)0x1000e010)
#define IGR_R_D_ENABLER ((volatile u32 *)0x1000f520)
#define IGR_R_D_ENABLEW ((volatile u32 *)0x1000f590)
#define IGR_R_D0_CHCR   ((volatile u32 *)0x10008000)
#define IGR_R_D1_CHCR   ((volatile u32 *)0x10009000)
#define IGR_R_D2_CHCR   ((volatile u32 *)0x1000a000)
#define IGR_R_D3_CHCR   ((volatile u32 *)0x1000b000)
#define IGR_R_D4_CHCR   ((volatile u32 *)0x1000b400)
#define IGR_R_D8_CHCR   ((volatile u32 *)0x1000d000)
#define IGR_R_D9_CHCR   ((volatile u32 *)0x1000d400)
#define IGR_R_GS_CSR    ((volatile u64 *)0x12001000)

// Retain the hardware checkpoints for explicit -dbc troubleshooting without
// flashing diagnostic colors during ordinary production use.
#define IGR_DIAG_COLOR(color) do { \
    if (eec.flags & EECORE_FLAG_DBC) \
        *GS_REG_BGCOLOR = (color); \
} while (0)

// Interrupt-context ResetEE. ps2sdk exposes only ResetEE() (a normal
// syscall); from inside an interrupt handler the interrupt variant must be
// used, which the EE kernel selects via the negated syscall number
// (__NR_ResetEE == 1).
static inline void iResetEE(u32 init_bitfield)
{
    __asm__ volatile(
        "move  $a0, %0\n"
        "li    $v1, -1\n"
        "syscall\n"
        "nop\n"
        :
        : "r"(init_bitfield)
        : "a0", "v1", "memory");
}

// scePadPortOpen / scePad2CreateSocket originals (captured during scan)
static int (*scePadPortOpen)(int port, int slot, void *addr);
static int (*scePad2CreateSocket)(pad2socketparam_t *SocketParam, void *addr);

#define IGR_PHYSICAL_PAD_COUNT 2

// One record for each physical controller socket. Multitap slots are
// intentionally out of scope: index 0 is port 0/slot 0 and index 1 is
// port 1/slot 0.
static paddata_t Pad_Data[IGR_PHYSICAL_PAD_COUNT];
static u16 IGR_Libpad = IGR_LIBPAD_NONE;
static u16 IGR_Libversion = 0;
static volatile u8 IGR_Reset_Requested = 0;

volatile int IGR_Thread_ID = -1;
static int IGR_Intc_ID = -1;
static int IGR_Scanner_Thread_ID = -1;

#define IGR_STACK_SIZE (4 * 1024)
static u8 IGR_Stack[IGR_STACK_SIZE] __attribute__((aligned(16)));
#define IGR_SCANNER_STACK_SIZE (1 * 1024)
static u8 IGR_Scanner_Stack[IGR_SCANNER_STACK_SIZE] __attribute__((aligned(16)));

extern void *_gp;
extern void *_stack_end;
extern int _iop_reboot_count;
extern u32 (*Old_SifSetDma)(SifDmaTransfer_t *sdd, s32 len);
extern int (*Old_SifSetReg)(u32 register_num, int register_value);

// Resident storage for the IGR reset command. Keeping this out of the thread
// stack also keeps the raw reset sequence small after a game has replaced its
// normal EE execution environment.
static SifCmdResetData_t IGR_Reset_Packet __attribute__((aligned(64)));

/*
 * Load NHDDL from a fresh ExecPS2 context.
 *
 * This intentionally does not run in the awakened game-side IGR thread. The
 * kernel ExecPS2 transition destroys the game's threads/semaphores, resets EE
 * peripheral state and gives this resident entry point a clean stack and GP.
 * OPL uses the same two-stage design (IGR_Exit -> ExecPS2(t_loadElf) ->
 * LoadElf -> ExecPS2(exit ELF)). Trying to bind LOADFILE and load NHDDL
 * directly from the old priority-0 IGR thread consistently reached the final
 * light-blue checkpoint and then froze on real hardware.
 */
static void IGR_Load_Dashboard(void)
{
    char *dashboardArgv[] = {eec.IGRExitPath};
    t_ExecData elf;
    int result;

    IGR_DIAG_COLOR(COLOR_MAGENTA); // Clean loader entry reached.

    SifInitRpc(0);
    result = SifInitIopHeap();
    if (result < 0) {
        IGR_DIAG_COLOR(COLOR_RED);
        for (;;) {
        }
    }

    result = SifLoadFileInit();
    if (result < 0) {
        IGR_DIAG_COLOR(COLOR_PURPLE);
        for (;;) {
        }
    }

    sbv_patch_disable_prefix_check();
    IGR_DIAG_COLOR(COLOR_YELLOW);

    // Restore the ROM memory-card stack required by the IOP-side ELF loader.
    SifLoadModule("rom0:SIO2MAN", 0, NULL);
    SifLoadModule("rom0:MCMAN", 0, NULL);
    SifLoadModule("rom0:MCSERV", 0, NULL);

    /*
     * ee_core96 and its dedicated stack end at 0x000A7000. The emulation
     * module staging area begins there and is no longer needed after the IOP
     * reset. Clear it together with all stale game RAM before loading NHDDL,
     * exactly as OPL clears everything above its resident loader boundary.
     */
    WipeUserMemory((void *)&_stack_end, (void *)GetMemorySize());
    FlushCache(WRITEBACK_DCACHE);

    IGR_DIAG_COLOR(COLOR_TEAL); // About to request the configured launcher.
    elf.epc = 0;
    elf.gp = 0;
    result = SifLoadElf(eec.IGRExitPath, &elf);
    if ((result == 0) && (elf.epc != 0)) {
        IGR_DIAG_COLOR(COLOR_GREEN); // Launcher is resident; execute it.
        SifLoadFileExit();
        SifExitIopHeap();
        SifExitRpc();
        FlushCache(WRITEBACK_DCACHE);
        FlushCache(INVALIDATE_ICACHE);
        CleanExecPS2((void *)elf.epc, (void *)elf.gp, 1, dashboardArgv);
    }

    // A steady red screen now means the memory-card ELF load returned/faulted.
    IGR_DIAG_COLOR(COLOR_RED);
    SifLoadFileExit();
    SifExitIopHeap();
    SifExitRpc();
    for (;;) {
    }
}

/*
 * Some games link libpad after their main ELF has started.  OPL retries from
 * its syscall hooks, but a title can load the library without creating one of
 * the narrow high-priority thread shapes that hook watches.  Once IGR has a
 * safe post-ExecPS2 foothold, retry at low priority during the game's startup
 * window.  Stop after 15 seconds so unsupported pad libraries do not cost CPU
 * for the rest of a play session.
 */
static void IGR_Scanner_Thread(void *arg)
{
    int attempt;

    (void)arg;
    for (attempt = 0; attempt < 150 && !padOpen_hooked; attempt++) {
        if (!disable_padOpen_hook) {
            padOpen_hooked = Install_PadOpen_Hook(0x00100000, 0x01ff0000, PADOPEN_HOOK);
            if (padOpen_hooked) {
                FlushCache(WRITEBACK_DCACHE);
                FlushCache(INVALIDATE_ICACHE);
                break;
            }
        }
        delay(100);
    }

    IGR_Scanner_Thread_ID = -1;
    ExitDeleteThread();
}

// The IGR thread: sleeps until the VBLANK handler detects the combo, then
// performs a clean IOP reset and relaunches the configured exit ELF.
static void IGR_Thread(void *arg)
{
    (void)arg;
    u32 cop0Perf;
    struct t_SifDmaTransfer igrDma;

    // Woken by the IGR interrupt handler on combo detect
    SleepThread();

    DPRINTF("IGR: thread woken, resetting to dashboard\n");

    // These colors survive the game's frozen frame and identify the last
    // completed reset stage on hardware. Successful resets only flash them.
    IGR_DIAG_COLOR(COLOR_WHITE);

    // The former Neutrino-specific synchronous shutdown RPC consistently
    // returned and then left the raw reset submission frozen at the blue
    // hardware checkpoint. Neutrino's FHI backend has no OPL DeviceLock /
    // DeviceUnmount contract, so that RPC was not equivalent to OPL's even
    // though it reused OPL's RPC number. At this point every game EE thread is
    // already suspended and SIF channels 5/6/7 deliberately remain enabled.
    // Give an already-submitted UDPFS/FHI transfer time to finish, then reset
    // the IOP directly without injecting another RPC into the SIF command
    // channel immediately beforehand.
    IGR_DIAG_COLOR(COLOR_MAGENTA);
    delay(250);
    IGR_DIAG_COLOR(COLOR_BLUE);

    /*
     * Submit the OPL-style raw IOP reset inline. The prior builds called the
     * resident Reset_Iop() function here; on hardware both reset inputs
     * consistently stopped on this dark-blue checkpoint without reaching the
     * function's first cyan store. The handler and this thread are therefore
     * alive, but that separate call target is not dependable in every game.
     * Keep the reset in this already-running resident function and use the
     * original SIF syscall handlers saved before the game installed its own
     * environment. As in OPL, SIF0 is deliberately not stopped.
     */
    IGR_DIAG_COLOR(COLOR_LBLUE);
    delay(100);
    _iop_reboot_count++;
    memset(&IGR_Reset_Packet, 0, sizeof(IGR_Reset_Packet));
    IGR_Reset_Packet.header.psize = sizeof(IGR_Reset_Packet);
    IGR_Reset_Packet.header.cid = SIF_CMD_RESET_CMD;
    IGR_Reset_Packet.arglen = 0;
    IGR_Reset_Packet.mode = 0;

    igrDma.src = &IGR_Reset_Packet;
    /*
     * This lookup happens in normal thread/user mode, so it must cross the
     * syscall boundary. Old_SifGetReg is the raw kernel handler captured by
     * Install_Kernel_Hooks(); calling it directly here jumps
     * into a kernel-only routine from user mode and freezes at the light-blue
     * checkpoint. OPL's Reset_Iop uses SifGetReg() here for the same reason.
     * The saved Old_SifSetReg/Old_SifSetDma handlers below are safe because
     * those calls occur only after ee_kmode_enter().
     */
    igrDma.dest = (void *)SifGetReg(SIF_SYSREG_SUBADDR);
    igrDma.size = sizeof(IGR_Reset_Packet);
    igrDma.attr = SIF_DMA_ERT | SIF_DMA_INT_O;
    SifWriteBackDCache(&IGR_Reset_Packet, sizeof(IGR_Reset_Packet));

    IGR_DIAG_COLOR(COLOR_WHITE);
    DIntr();
    ee_kmode_enter();
    Old_SifSetReg(SIF_REG_SMFLAG, SIF_STAT_BOOTEND);
    while (!Old_SifSetDma(&igrDma, 1)) {
        IGR_DIAG_COLOR(COLOR_RED);
        ee_kmode_exit();
        EIntr();
        delay(1);
        DIntr();
        ee_kmode_enter();
        Old_SifSetReg(SIF_REG_SMFLAG, SIF_STAT_BOOTEND);
    }
    Old_SifSetReg(SIF_REG_SMFLAG, SIF_STAT_SIFINIT);
    Old_SifSetReg(SIF_REG_SMFLAG, SIF_STAT_CMDINIT);
    Old_SifSetReg(SIF_SYSREG_RPCINIT, 0);
    Old_SifSetReg(SIF_SYSREG_SUBADDR, (int)NULL);
    ee_kmode_exit();
    EIntr();

    IGR_DIAG_COLOR(COLOR_YELLOW);
    Remove_Kernel_Hooks();
    IGR_DIAG_COLOR(COLOR_PURPLE);

    // Some games (notably GT4/GTA) replace the EE memory map. Restore the
    // standard TLB before touching/loading application memory, matching OPL's
    // reset path and current ps2sdk ExecPS2 behaviour.
    InitializeTLB();
    IGR_DIAG_COLOR(COLOR_TEAL);

    // Games such as GT4/GTA leave the COP0 performance counter running; an
    // overflow during the next application can raise an exception. Restore
    // the other resident hooks before loading the dashboard as OPL IGR does.
    cop0Perf = GetCop0(25);
    if (cop0Perf & 0x80000000) {
        __asm__ volatile(
            "mfc0 $3, $25\n"
            "lui  $2, 0x8000\n"
            "or   $3, $3, $2\n"
            "xor  $3, $3, $2\n"
            "mtc0 $3, $25\n"
            "sync.p\n"
            :
            :
            : "$2", "$3", "memory");
    }
    if (eec.GsmVideoMode != EECORE_GSM_VMODE_NONE)
        DisableGSM();
    if (eec.CheatList != NULL)
        DisableCheats();
    IGR_DIAG_COLOR(COLOR_OLIVE);

    // Wait for the raw reset submitted above to finish.
    while (!SifIopSync()) {
    }
    IGR_DIAG_COLOR(COLOR_GREEN);

    /*
     * Do not initialize memory-card RPC or load the dashboard from this old
     * game thread. Enter a clean EE execution context first, matching OPL's
     * IGR_Exit/t_loadElf handoff. Hooks were removed above, so this reaches
     * the original ExecPS2 implementation rather than Neutrino's game hook.
     */
    IGR_DIAG_COLOR(COLOR_WHITE);

    /*
     * Match OPL's IGR_Exit handoff exactly here. Neutrino's CleanExecPS2
     * wrapper is not suitable for this first transition because it
     * clears the caller's register set before entering the kernel and real
     * hardware stopped after the game disappeared, leaving a black screen
     * before the loader's first checkpoint.  ExecPS2 already creates the
     * clean thread/peripheral context this resident loader needs.  Keep
     * CleanExecPS2 only for the second transition, after NHDDL has been loaded
     * into memory, where it is Neutrino's established ELF-launch path.
     */
    if (eec.IGRExitPath[0] != '\0')
        ExecPS2((void *)IGR_Load_Dashboard, &_gp, 0, NULL);

    // Match upstream OPL: no configured exit ELF returns to the PS2 Browser.
    Exit(0);
}

// VBLANK-END interrupt handler: watches the game's pad buffer for the combo.
static int IGR_Intc_Handler(int cause)
{
    (void)cause;
    int pad_index;
    int liveness_sampled = 0;
    int any_pad_live = 0;

    for (pad_index = 0; pad_index < IGR_PHYSICAL_PAD_COUNT; pad_index++) {
        paddata_t *pad = &Pad_Data[pad_index];

        if (pad->pad_buf != NULL) {
            u8 *pad_buf = (u8 *)UNCACHED_SEG(pad->pad_buf);
            u8 pad_pos_state = pad_buf[pad->pos_state];
            u8 pad_pos_frame = pad_buf[pad->pos_frame];
            u8 pad_pos_combo1 = pad_buf[pad->pos_combo1];
            u8 pad_pos_combo2 = pad_buf[pad->pos_combo2];

            if (((pad->libpad == IGR_LIBPAD) && (pad_pos_state == IGR_PAD_STABLE_V1)) ||
                ((pad->libpad == IGR_LIBPAD2) && (pad_pos_state == IGR_PAD_STABLE_V2))) {
                // Track each socket independently. A stale controller-1
                // buffer must not hide a live controller-2 buffer (or vice
                // versa) and spuriously re-arm the code scanner.
                if (pad->vb_count++ >= 10) {
                    pad->live = (pad_pos_frame != pad->prev_frame);
                    if (pad->live)
                        pad->prev_frame = pad_pos_frame;
                    pad->vb_count = 0;
                    liveness_sampled = 1;
                }

                if (pad_pos_combo1 == IGR_COMBO_R1_L1_R2_L2 &&
                    pad_pos_combo2 == IGR_COMBO_START_SELECT)
                    IGR_Reset_Requested = pad_pos_combo2;
            }
        } else {
            pad->live = 0;
        }

        if (pad->live)
            any_pad_live = 1;
    }

    if (liveness_sampled)
        padOpen_hooked = any_pad_live;

    // Catch the console's front-panel reset/power event even for titles whose
    // libpad implementation could not be hooked. Cancel the hardware's normal
    // poweroff command and route a tap through the same clean reset path as
    // the controller combo.
    if (eec.IGRFlags & EECORE_IGR_FLAG_POWER_BUTTON_RESET) {
        ee_kmode_enter();
        if ((*CDVD_R_NDIN & 0x20) && (*CDVD_R_POFF & 0x04)) {
            *CDVD_R_SDIN = 0x00;
            *CDVD_R_SCMD = 0x1B;
            IGR_Reset_Requested = IGR_COMBO_START_SELECT;
        }
        ee_kmode_exit();
    }

    if (IGR_Reset_Requested != 0x00) {
        int i;
        int wake_result;
        ee_thread_status_t worker_status;
        u32 gs_reset_polls;

        // Some games terminate/delete threads they did not create and then
        // reuse the ID. Never freeze the game unless the saved ID still
        // describes our exact resident worker and a state that can be woken.
        if ((IGR_Thread_ID <= 0) || (IGR_Thread_ID >= 256) ||
            (iReferThreadStatus(IGR_Thread_ID, &worker_status) < 0) ||
            (worker_status.func != IGR_Thread) ||
            (worker_status.stack != (void *)IGR_Stack) ||
            (worker_status.stack_size != IGR_STACK_SIZE)) {
            IGR_Reset_Requested = 0x00;
            ExitHandler();
            return 0;
        }

        if ((worker_status.status == THS_WAITSUSPEND) ||
            (worker_status.status == THS_SUSPEND)) {
            if (iResumeThread(IGR_Thread_ID) < 0) {
                IGR_Reset_Requested = 0x00;
                ExitHandler();
                return 0;
            }
        } else if (!(((worker_status.status == THS_WAIT) &&
                      (worker_status.waitType == TSW_SLEEP)) ||
                     (worker_status.status == THS_READY))) {
            IGR_Reset_Requested = 0x00;
            ExitHandler();
            return 0;
        }

        // Interrupt dispatch cannot run the worker until ExitHandler(). Wake
        // it first so a failed wake leaves the game intact instead of frozen
        // after DMA, GS, and every other thread have already been stopped.
        wake_result = iWakeupThread(IGR_Thread_ID);
        if (wake_result < 0) {
            IGR_Reset_Requested = 0x00;
            ExitHandler();
            return 0;
        }

        // Bring the EE to a safe state before waking the IGR thread.
        // (Mirrors OPL: stop DMA, reset GS, ResetEE from the handler.)
        asm volatile("sync.l\n");

        u32 dmaEnableR = *IGR_R_D_ENABLER;
        *IGR_R_D_ENABLEW = dmaEnableR | 0x10000;
        (void)*IGR_R_D_CTRL;
        (void)*IGR_R_D_STAT;
        *IGR_R_D0_CHCR = 0;
        *IGR_R_D1_CHCR = 0;
        *IGR_R_D2_CHCR = 0;
        *IGR_R_D3_CHCR = 0;
        *IGR_R_D4_CHCR = 0;
        *IGR_R_D8_CHCR = 0;
        *IGR_R_D9_CHCR = 0;
        *IGR_R_D_ENABLEW = dmaEnableR;

        asm volatile("sync.l\n");

        *IGR_R_GS_CSR = 0x100; // Reset GS
        asm volatile("sync.l\n");

        if (_strcmp(eec.GameID, "SLUS_200.70") == 0) {
            // Q-Ball can leave the GS RESET bit asserted forever. ResetEE
            // immediately below resets the same peripheral state, so bound
            // this one title's poll and fail forward to the resident worker.
            gs_reset_polls = 0x100000;
            while ((*IGR_R_GS_CSR & 0x100) && --gs_reset_polls) {
            }
        } else {
            // Preserve the hardware-proven production sequence for titles
            // that have never exhibited Q-Ball's stuck-GS condition.
            while (*IGR_R_GS_CSR & 0x100) {
            }
        }

        iResetEE(0x7F);

        // Suspend every thread except idle and our IGR thread
        for (i = 1; i < 256; i++) {
            if (i != IGR_Thread_ID)
                iSuspendThread(i);
        }

        iChangeThreadPriority(IGR_Thread_ID, 0);
        // Retain the proven post-takeover wake as well. The earlier wake is a
        // fail-safe validation; this second one preserves the established
        // production ordering after ResetEE and thread suspension.
        iWakeupThread(IGR_Thread_ID);
    }

    ExitHandler();
    return 0;
}

static int IGR_Pad_Index(int port, int slot)
{
    if ((slot == 0) && (port >= 0) && (port < IGR_PHYSICAL_PAD_COUNT))
        return port;

    return -1;
}

// Sets pad-buffer field offsets for the detected libpad version.
static void Set_libpad_Params(int pad_index, void *addr)
{
    paddata_t *pad;

    if ((pad_index < 0) || (pad_index >= IGR_PHYSICAL_PAD_COUNT))
        return;

    pad = &Pad_Data[pad_index];
    DI();

    // Publish pad_buf last so the VBLANK handler can never observe a new
    // pointer paired with partially initialized offsets or library metadata.
    pad->pad_buf = NULL;
    pad->libpad = IGR_Libpad;
    pad->libversion = IGR_Libversion;
    pad->vb_count = 0;
    pad->combo_type = 0x00;
    pad->prev_frame = 0x00;
    pad->live = 0;

    if (pad->libpad == IGR_LIBPAD) {
        if (pad->libversion >= 0x0160) {
            pad->pos_combo1 = 3;
            pad->pos_combo2 = 2;
            pad->pos_state = 112;
            pad->pos_frame = 88;
        } else {
            pad->pos_combo1 = 11;
            pad->pos_combo2 = 10;
            pad->pos_state = 4;
            pad->pos_frame = 0;
        }
    } else if (pad->libpad == IGR_LIBPAD2) {
        pad->pos_combo1 = 29;
        pad->pos_combo2 = 28;
        pad->pos_state = 4;
        pad->pos_frame = 124;
    }

    pad->pad_buf = addr;

    EI();
}

void Install_IGR(void)
{
    ee_thread_t thread_param;
    int start_result;

    if (IGR_Thread_ID < 0) {
        // Initialize state only when creating a new post-ExecPS2 IGR instance.
        // Install_IGR is intentionally safe to call repeatedly from syscall
        // lifecycle hooks so the physical button does not depend on libpad.
        memset(Pad_Data, 0, sizeof(Pad_Data));
        IGR_Reset_Requested = 0x00;

        thread_param.gp_reg = &_gp;
        thread_param.func = IGR_Thread;
        thread_param.stack = (void *)IGR_Stack;
        thread_param.stack_size = IGR_STACK_SIZE;
        thread_param.initial_priority = 127;
        thread_param.attr = 0;
        thread_param.option = 0;
        IGR_Thread_ID = CreateThread(&thread_param);
        if (IGR_Thread_ID >= 0) {
            start_result = StartThread(IGR_Thread_ID, NULL);
            if (start_result < 0) {
                DeleteThread(IGR_Thread_ID);
                IGR_Thread_ID = -1;
            }
        }
    }

    // Never install a destructive reset interrupt without a valid worker.
    if ((IGR_Thread_ID > 0) && (IGR_Intc_ID < 0)) {
        IGR_Intc_ID = AddIntcHandler(kINTC_VBLANK_END, IGR_Intc_Handler, 0);
        if (IGR_Intc_ID >= 0)
            EnableIntc(kINTC_VBLANK_END);
    }

    if (!padOpen_hooked && IGR_Scanner_Thread_ID < 0) {
        thread_param.gp_reg = &_gp;
        thread_param.func = IGR_Scanner_Thread;
        thread_param.stack = (void *)IGR_Scanner_Stack;
        thread_param.stack_size = IGR_SCANNER_STACK_SIZE;
        thread_param.initial_priority = 127;
        thread_param.attr = 0;
        thread_param.option = 0;
        IGR_Scanner_Thread_ID = CreateThread(&thread_param);
        if (IGR_Scanner_Thread_ID >= 0)
            StartThread(IGR_Scanner_Thread_ID, NULL);
    }
}

void Reset_Padhook(void)
{
    IGR_Intc_ID = -1;
    IGR_Thread_ID = -1;
    IGR_Scanner_Thread_ID = -1;
    IGR_Reset_Requested = 0x00;
    memset(Pad_Data, 0, sizeof(Pad_Data));
    padOpen_hooked = 0;
}

// Hook for libpad scePadPortOpen: re-checks the hook and installs IGR.
static int Hook_scePadPortOpen(int port, int slot, void *addr)
{
    int ret;
    int pad_index = IGR_Pad_Index(port, slot);

    if (pad_index >= 0)
        Install_PadOpen_Hook(0x00100000, 0x01ff0000, PADOPEN_CHECK);

    ret = scePadPortOpen(port, slot, addr);

    if (pad_index >= 0) {
        Install_IGR();
        Set_libpad_Params(pad_index, addr);
    }

    return ret;
}

// Hook for libpad2 scePad2CreateSocket.
static int Hook_scePad2CreateSocket(pad2socketparam_t *SocketParam, void *addr)
{
    int ret;
    int pad_index = (SocketParam == NULL) ? 0 :
                    IGR_Pad_Index(SocketParam->port, SocketParam->slot);

    if (pad_index >= 0)
        Install_PadOpen_Hook(0x00100000, 0x01ff0000, PADOPEN_CHECK);

    ret = scePad2CreateSocket(SocketParam, addr);

    if (pad_index >= 0) {
        Install_IGR();
        Set_libpad_Params(pad_index, addr);
    }

    return ret;
}

int Install_PadOpen_Hook(u32 mem_start, u32 mem_end, int mode)
{
    u32 *ptr, *ptr2;
    u32 inst, fncall;
    u32 mem_size, mem_size2;
    u32 pattern[1], mask[1];
    int i, found, patched;

    pattern_t padopen_patterns[NB_PADOPEN_PATTERN] = {
        {padPortOpenpattern0, padPortOpenpattern0_mask, sizeof(padPortOpenpattern0), 1, 0x0211},
        {pad2CreateSocketpattern0, pad2CreateSocketpattern0_mask, sizeof(pad2CreateSocketpattern0), 2, 0x0200},
        {pad2CreateSocketpattern1, pad2CreateSocketpattern1_mask, sizeof(pad2CreateSocketpattern1), 2, 0x0200},
        {pad2CreateSocketpattern2, pad2CreateSocketpattern2_mask, sizeof(pad2CreateSocketpattern2), 2, 0x0200},
        {padPortOpenpattern1, padPortOpenpattern1_mask, sizeof(padPortOpenpattern1), 1, 0x0210},
        {padPortOpenpattern2, padPortOpenpattern2_mask, sizeof(padPortOpenpattern2), 1, 0x0160},
        {padPortOpenpattern3, padPortOpenpattern3_mask, sizeof(padPortOpenpattern3), 1, 0x0150}};

    found = 0;
    patched = 0;

    for (i = 0; i < NB_PADOPEN_PATTERN; i++) {
        ptr = (u32 *)mem_start;
        while (ptr) {
            mem_size = mem_end - (u32)ptr;

            ptr = find_pattern_with_mask(ptr, mem_size, padopen_patterns[i].pattern, padopen_patterns[i].mask, padopen_patterns[i].size);
            if (ptr) {
                DPRINTF("IGR: found padopen pattern%d at 0x%08x mode=%d\n", i, (int)ptr, mode);
                found = 1;

                if (padopen_patterns[i].type == IGR_LIBPAD)
                    scePadPortOpen = (void *)ptr;
                else
                    scePad2CreateSocket = (void *)ptr;

                if (mode == PADOPEN_HOOK) {
                    // Match both J and JAL to the located function
                    inst = 0x08000000 | (0x03ffffff & ((u32)ptr >> 2));
                    pattern[0] = inst;
                    mask[0] = 0xfbffffff;

                    ptr2 = (u32 *)mem_start;
                    while (ptr2) {
                        mem_size2 = (u32)((u8 *)mem_end - (u8 *)ptr2);
                        ptr2 = find_pattern_with_mask(ptr2, mem_size2, pattern, mask, sizeof(pattern));
                        if (ptr2) {
                            patched = 1;
                            fncall = (u32)ptr2;
                            inst = (ptr2[0] & 0xfc000000);
                            if (padopen_patterns[i].type == IGR_LIBPAD)
                                inst |= 0x03ffffff & ((u32)Hook_scePadPortOpen >> 2);
                            else
                                inst |= 0x03ffffff & ((u32)Hook_scePad2CreateSocket >> 2);
                            _sw(inst, fncall);
                            IGR_Libpad = padopen_patterns[i].type;
                            IGR_Libversion = padopen_patterns[i].version;
                        }
                    }

                    // Fallback: patch pointer-to-function (JALR use)
                    if (!patched) {
                        pattern[0] = (u32)ptr;
                        mask[0] = 0xffffffff;
                        ptr2 = (u32 *)mem_start;
                        while (ptr2) {
                            mem_size2 = (u32)((u8 *)mem_end - (u8 *)ptr2);
                            ptr2 = find_pattern_with_mask(ptr2, mem_size2, pattern, mask, sizeof(pattern));
                            if (ptr2) {
                                patched = 1;
                                fncall = (u32)ptr2;
                                if (padopen_patterns[i].type == IGR_LIBPAD)
                                    inst = (u32)Hook_scePadPortOpen;
                                else
                                    inst = (u32)Hook_scePad2CreateSocket;
                                _sw(inst, fncall);
                                IGR_Libpad = padopen_patterns[i].type;
                                IGR_Libversion = padopen_patterns[i].version;
                            }
                        }
                    }
                } else {
                    break; // PADOPEN_CHECK: found is enough
                }

                ptr += (padopen_patterns[i].size >> 2);
            }
        }

        if (patched == 1 || (mode == PADOPEN_CHECK && found == 1))
            break;
    }

    return patched;
}
