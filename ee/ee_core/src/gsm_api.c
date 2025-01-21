#include <tamtypes.h>
#include <kernel.h>
#include <syscallnr.h>
#include <ee_debug.h>
#include <gs_privileged.h> // BGCOLOR

#include "ee_core.h"
#include "ee_core_flag.h"

#include "ee_asm.h"
#include "ee_regs.h"
#include "ee_exception_l2.h"

#define MAKE_J(func) (u32)((0x02 << 26) | (((u32)func) / 4)) // Jump (MIPS instruction)
#define NOP          0x00000000                              // No Operation (MIPS instruction)

#define _TO_KSEG0(x) ((void *)(((u32)(x) & 0x0fffffff) | 0x80000000))
#define _TO_KSEG1(x) ((void *)(((u32)(x) & 0x0fffffff) | 0xA0000000))


// Registers from interrupted program
u8 el2_stack[256] __attribute__ ((aligned(128)));

// Registers from interrupted program
ee_registers_t el2_regs __attribute__ ((aligned(128)));

typedef void (*fp_SetGsCrt)(short int interlace, short int mode, short int ffmd);

// GSM state
struct gsm_state {
    fp_SetGsCrt org_SetGsCrt;

    u32 SetGsCrt : 1;
    u32 LineDouble : 1;
    u32 Spare : 30;
};
static struct gsm_state state = {
    NULL,
    0, 0, 0,
};

enum MIPS_OP {
    MOP_SPECIAL = 0x00,
    MOP_REGIMM  = 0x01,
    MOP_J       = 0x02,
    MOP_JAL     = 0x03,
    MOP_BEQ     = 0x04,
    MOP_BNE     = 0x05,
    MOP_BLEZ    = 0x06,
    MOP_BGTZ    = 0x07,
    MOP_COP1    = 0x11,
    MOP_BEQL    = 0x14,
    MOP_BNEL    = 0x15,
    MOP_BLEZL   = 0x16,
    MOP_BGTZL   = 0x17,
    MOP_MMI     = 0x1C,
    MOP_SD      = 0x3f
};

enum MIPS_OP_SPECIAL {
    MOPS_SLL  = 0x00,
    MOPS_JR   = 0x08,
    MOPS_JALR = 0x09
};

enum MIPS_OP_IMM {
    MOPI_BLTZ    = 0x00,
    MOPI_BGEZ    = 0x01,
    MOPI_BLTZL   = 0x02,
    MOPI_BGEZL   = 0x03,
    MOPI_BLTZAL  = 0x10,
    MOPI_BGEZAL  = 0x11,
    MOPI_BLTZALL = 0x12,
    MOPI_BGEZALL = 0x13,
};

/*
 * Devide x/width by 2 if possible
 *
 * BufferWidth  MAGH+1  MAGH-new
 * -----------------------------
 * 256          10      5 = perfect
 * 320           8      4 = perfect
 * 384           7      3 = too small, perhaps not bad when stretched on a modern 16:9 TV
 * 512           5      2 = too small, perhaps not bad when stretched on a modern 16:9 TV
 * 640           4      2 = perfect
 *
 */
static u64 mod_DISPLAY(struct gsm_state *pstate, u64 r)
{
    u32 dx, dy, magh, magv, dw, dh;

    dx   = (r      ) & 0xfff;
    dy   = (r >> 12) & 0x7ff;
    magh = (r >> 23) & 0x00f;
    magv = (r >> 27) & 0x003;
    dw   = (r >> 32) & 0xfff;
    dh   = (r >> 44) & 0x7ff;

    if (pstate->LineDouble) {
        // From 288p/240p to 576p/480p
        magv = ((magv + 1) * 2) - 1;
    }

    // From PAL/NTSC to 576p/480p, the VCK units are twice as small

    // Divide MAGH by 2
    u32 magh_new = ((magh + 1) / 2) - 1;

    // Rescale width to match new MAGH
    u32 dw_new = ((dw + 1) * (magh_new + 1) / (magh + 1)) - 1;

    // Re-adjust offset by compensating for both:
    // - VCK change (/2)
    // - possible in-perfect MAGH change making the image smaller
    dx /= 2;
    dx += (((dw + 1) / 2) - (dw_new + 1)) / 2;

    return GS_SET_DISPLAY(dx, dy, magh_new, magv, dw_new, dh);
}

// Rainbow color scheme
static u64 debug_color[] = {
    GS_SET_BGCOLOR(255,   0,   0), // 0 = Red
    GS_SET_BGCOLOR(255, 128,   0), // 1 = Orange
    GS_SET_BGCOLOR(255, 255,   0), // 2 = Yellow
    GS_SET_BGCOLOR(  0, 255,   0), // 3 = Green
    GS_SET_BGCOLOR(  0, 255, 255), // 4 = L-Blue
    GS_SET_BGCOLOR(  0,   0, 255), // 5 = D-Blue
    GS_SET_BGCOLOR(127,   0, 255), // 6 = Purple

    GS_SET_BGCOLOR(127, 127, 127), // 7 = Grey
    GS_SET_BGCOLOR(255, 255, 255), // 8 = White
};

/*
 * Exception Level 2 handler
 *   Status.ERL = 1
 *   kuseg region is unmapped uncached !!! - 2048MiB, offset 0
 *   kseg0 region is unmapped cached       -  512MiB, offset 0x80000000
 *   kseg1 region is unmapped uncached     -  512MiB, offset 0xA0000000
 *
 *   Things to watch out for:
 *   - code is compiled for: useg   !FIXME! for now just don't use global data in this function.
 *   - code runs in:         kseg0
 *   - stack located at:     kseg0
 */
void el2_c_handler(ee_registers_t *regs)
{
    struct gsm_state *pstate = _TO_KSEG0(&state);
    u32 cop0_Cause = _ee_mfc0(EE_COP0_Cause);
    u32 cop0_ErrorEPC = _ee_mfc0(EE_COP0_ErrorEPC);

    // Check for debug exception
    if (M_EE_GET_CAUSE_EXC2(cop0_Cause) != EE_EXC2_DBG) {
        *GS_REG_BGCOLOR = debug_color[0]; // Red
        _ee_disable_bpc();
        while(1){}
        return;
    }

    // Get the instruction that triggered the exception
    u32 instr;
    if (cop0_Cause & EE_CAUSE_BD2)
        instr = *(u32*)(cop0_ErrorEPC + 4); // Branch delay slot
    else
        instr = *(u32*)(cop0_ErrorEPC);

    // Decode the instruction
    u32 op  = (instr >> 26) & 0x003f;
    u32 rs  = (instr >> 21) & 0x001f;
    u32 rt  = (instr >> 16) & 0x001f;
    u32 imm = (instr      ) & 0xffff;
    u64 *dest;
    u64 value;
    if (op == MOP_SD) {
        value = (u64)regs->gpr[rt];
        dest  = (u64*)((u32)regs->gpr[rs] + imm);
    } else {
        *GS_REG_BGCOLOR = debug_color[1]; // Orange
        _ee_disable_bpc();
        while(1){}
        return;
    }

    // Disable breakpoint while handling GS registers
    u32 bpc_save = _ee_disable_bpc();

    // Store the written value
    switch((u32)dest & 0x1fffffff) {
        case (u32)GS_REG_DISPLAY1:
            *GS_REG_DISPLAY1 = mod_DISPLAY(pstate, value);
            break;
        case (u32)GS_REG_DISPLAY2:
            *GS_REG_DISPLAY2 = mod_DISPLAY(pstate, value);
            break;
        case (u32)GS_REG_SMODE2:
            // Check if line-doubling is needed
            if (pstate->SetGsCrt)
                pstate->SetGsCrt = 0;
            else
                pstate->LineDouble = (value & 2) ? 1 : 0;
            *GS_REG_SMODE2 = GS_SET_SMODE2(0,1,0);
            break;
        default:
#if 1
            *dest = value;
#else
            *GS_REG_BGCOLOR = debug_color[2]; // Yellow
            //_ee_disable_bpc(); // already disabled
            while(1){}
            return;
#endif
    }

    // Make sure data is written to RAM
    _mem_barrier();

    // Restore breakpoints
    _ee_enable_bpc(bpc_save);

    // Advance PC
    u32 new_pc;
    if (cop0_Cause & EE_CAUSE_BD2) {
        // Branch delay slot, emulate the branch instruction
        u32 branch_instr = *(u32*)(cop0_ErrorEPC);
        // Decode the instruction
            op  = (branch_instr >> 26) & 0x003f; // OP
            rs  = (branch_instr >> 21) & 0x001f; // Register source
            rt  = (branch_instr >> 16) & 0x001f; // Register temp
        u32 ops = (branch_instr      ) & 0x003f; // Special OP
        s32 offset = (branch_instr & 0xffff);
        if (offset & 0x8000) {
            // Negative
            offset = ~((offset^0xffff) + 1) + 1;
        }
        u32 instr_index = (branch_instr << 2) & 0x0fffffff;
        int rlo; // Result of logical operation

        switch (op) {
            case MOP_SPECIAL:
                switch (ops) {
                    case MOPS_JALR:
                        regs->gpr[31] = cop0_ErrorEPC + 8;
                        new_pc = regs->gpr[rs];
                        break;
                    case MOPS_JR:
                        new_pc = regs->gpr[rs];
                        break;
                    default:
                        // Just freeze!
                        *GS_REG_BGCOLOR = debug_color[3]; // Green
                        while(1){}
                }
                break;
            case MOP_REGIMM:
                switch (rt) {
                    case MOPI_BLTZ:
                    case MOPI_BLTZL:
                        rlo = regs->gpr[rs] < 0;
                        goto handle_rlo;
                    case MOPI_BGEZ:
                    case MOPI_BGEZL:
                        rlo = regs->gpr[rs] >= 0;
                        goto handle_rlo;
                    case MOPI_BLTZAL:
                    case MOPI_BLTZALL:
                        regs->gpr[31] = cop0_ErrorEPC + 8;
                        rlo = regs->gpr[rs] < 0;
                        goto handle_rlo;
                    case MOPI_BGEZAL:
                    case MOPI_BGEZALL:
                        regs->gpr[31] = cop0_ErrorEPC + 8;
                        rlo = regs->gpr[rs] >= 0;
                        goto handle_rlo;
                    default:
                        // Just freeze!
                        *GS_REG_BGCOLOR = debug_color[4]; // L-Blue
                        while(1){}
                }
                break;
            case MOP_COP1:
                {
                    u32 fcr31 = _ee_cfc1_r31();
                    if ((branch_instr & 0x03ff0000) == 0x01000000) {
                        // BC1F - Branch on FP False
                        rlo = (fcr31 & 0x00800000) ? 0 : 1;
                    } else if ((branch_instr & 0x03ff0000) == 0x01010000) {
                        // BC1T - Branch on FP True
                        rlo = (fcr31 & 0x00800000) ? 1 : 0;
                    } else {
                        // Keep compiler happy
                        rlo = 0;
                    }
                }
                goto handle_rlo;
                break;
            case MOP_J:
                new_pc = instr_index;
                break;
            case MOP_JAL:
                regs->gpr[31] = cop0_ErrorEPC + 8;
                new_pc = instr_index;
                break;
            case MOP_BEQ:
            case MOP_BEQL:
                rlo = regs->gpr[rs] == regs->gpr[rt];
                goto handle_rlo;
            case MOP_BNE:
            case MOP_BNEL:
                rlo = regs->gpr[rs] != regs->gpr[rt];
                goto handle_rlo;
            case MOP_BLEZ:
            case MOP_BLEZL:
                rlo = regs->gpr[rs] <= 0;
                goto handle_rlo;
            case MOP_BGTZ:
            case MOP_BGTZL:
                rlo = regs->gpr[rs] > 0;
                goto handle_rlo;
            default:
                // Just freeze!
                *GS_REG_BGCOLOR = debug_color[5]; // D-Blue
                while(1){}
        }

        goto handle_rlo_done;
handle_rlo:
        if (rlo)
            new_pc = cop0_ErrorEPC + 4 + offset * 4;
        else
            new_pc = cop0_ErrorEPC + 8;
handle_rlo_done:

    } else {
        // Normal instruction (no branch delay slot)
        // Advance to next instruction
        new_pc = cop0_ErrorEPC + 4;
    }
    _ee_mtc0(EE_COP0_ErrorEPC, new_pc);
    _ee_sync_p();
}

/*
 * Kernel replacement function
 *
 *   Things to watch out for:
 *   - code is compiled for: useg   !FIXME! for now just don't use global data in this function.
 *   - code runs in:         kseg0
 *   - stack located at:     kseg0
 */
static void hook_SetGsCrt(short int interlace, short int mode, short int ffmd)
{
    struct gsm_state *pstate = _TO_KSEG0(&state);

    //printf("%s(%d, 0x%x, %d)\n", __FUNCTION__, interlace, mode, ffmd);

    // Only NTSC(2) and PAL(3) modes are supported
    if ((mode != 2) && (mode != 3))
        goto no_gsm;

    // 576p not supported
    if (mode == 3 && (g_ee_core_flags & EECORE_FLAG_GSM_NO_576P))
        goto no_gsm;

    // Only do line doubling if asked
    if (ffmd == 1 && (g_ee_core_flags & EECORE_FLAG_GSM2) == 0)
        goto no_gsm;

    pstate->LineDouble = ffmd;

    // Enable GSM
    interlace = 0;
    if (mode == 2) mode = 0x50; // 480p
    if (mode == 3) mode = 0x53; // 576p
    ffmd = 1;

    pstate->SetGsCrt = 1;
    _ee_enable_bpc(EE_BPC_DWE | EE_BPC_DUE | EE_BPC_DKE);
    pstate->org_SetGsCrt(interlace, mode, ffmd);
    return;

no_gsm:
    _ee_disable_bpc();
    pstate->org_SetGsCrt(interlace, mode, ffmd);
}

void EnableGSM(void)
{
    vu32 *v_debug = (vu32 *)0x80000100;

    // Hook SetGsCrt
    state.org_SetGsCrt = GetSyscallHandler(__NR_SetGsCrt);
    SetSyscall(__NR_SetGsCrt, (void *)(((u32)(hook_SetGsCrt) & ~0xE0000000) | 0x80000000));

    // Make sure no exceptions are generated
    _ee_disable_bpc();

    // Install level 2 exception handler
    ee_kmode_enter();
    v_debug[0] = MAKE_J((int)el2_asm_handler);
    v_debug[1] = NOP;
    ee_kmode_exit();
    FlushCache(WRITEBACK_DCACHE);
    FlushCache(INVALIDATE_ICACHE);

    // Set breakpoint for:
    //   0x12000020 = GS_REG_SMODE2   (write only)
    //   0x12000080 = GS_REG_DISPLAY1 (write only)
    //   0x120000a0 = GS_REG_DISPLAY2 (write only)
    //   0x1fffff5f = mask
    _ee_mtdab(0x12000000);
    _ee_mtdabm(0x1fffff5f);
    // Enable breakpoint in SetGsCrt
    //_ee_enable_bpc(EE_BPC_DWE | EE_BPC_DUE | EE_BPC_DKE);
}

void DisableGSM(void)
{
    // Make sure no exceptions are generated
    _ee_disable_bpc();
    // Restore syscalls
    SetSyscall(__NR_SetGsCrt, state.org_SetGsCrt);
}