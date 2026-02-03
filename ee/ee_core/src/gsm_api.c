// PS2SDK
#include <tamtypes.h>
#include <kernel.h>
#include <syscallnr.h>
#include <ee_debug.h>
#include <gs_privileged.h>

// Neutrino
#include "util.h"
#include "ee_debug.h"
#include "interface.h"
#include "ee_asm.h"
#include "ee_regs.h"
#include "ee_debug.h"
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

struct video_mode {
    u32 mode      : 7;
    u32 interlace : 1; // 0 = non-interlaced, 1 = interlaced
    u32 ffmd      : 1; // 0 = field, 1 = frame
    u32 vck       : 3;
    u32 reserved  : 20;

    u16 FBHeight; // 240/480/960 = 1/2/4
    u16 DPHeight;   // 240/480/960 = 1/2/4

    int x_center_vck;
    int y_center;
};

// GSM state
struct gsm_state {
    fp_SetGsCrt org_SetGsCrt;

    enum EECORE_GSM_VMODE GsmVideoMode;
    enum EECORE_GSM_COMP_MODE GsmCompMode;
    u32 flags;

    struct video_mode game;
    struct video_mode gsm;

    u32 VSINTCount :  4;
    u32 VSINTPrev  :  1;
    u32 VCKDiv     :  3; // VCK    devide   (1x, 2x     or 4x)
    u32 MAGHMul    :  3; // Width  multiply (1x, 2x, 3x or 4x)

    u32 FBMul    :  3; // Height multiply (1x, 2x     or 4x)
    u32 FBDiv    :  3; // Height devide   (1x or 2x)

    u32 DPMul      :  3; // Height multiply (1x, 2x     or 4x)
    u32 DPDiv      :  3; // Height devide   (1x or 2x)

    u64 last_display1;
    u64 last_display2;
};
static struct gsm_state state = {NULL};

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
    MOP_LD      = 0x37,
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
 * MAGH is 2x smaller for 480p/576p
 *
 * BufferWidth  MAGH+1 (old)      MAGH+1 (new)
 * --------------------------------------------
 * 256          * 10 / 4 = 640    * 5 / 2 = 640    ->    perfect
 * 320          *  8 / 4 = 640    * 4 / 2 = 640    ->    perfect
 * 384          *  7 / 4 = 640    * 3 / 2 = 576    ->    too small, perhaps not bad when stretched on a modern 16:9 TV
 * 512          *  5 / 4 = 640    * 2 / 2 = 512    ->    too small, perhaps not bad when stretched on a modern 16:9 TV
 * 640          *  4 / 4 = 640    * 2 / 2 = 640    ->    perfect
 *
 * 
 * MAGH is 4x smaller for 1080i, but twice as wide, so effectively 2x smaller
 * More stretch options available
 * 
 * Also the Pixel Aspect Ratio has to be taken into account:
 *      4:3   16:9
 * ---------------
 * PAL  1,09  1,45
 * NTSC 0,91  1,21
 * 
 * Resulting in ideal widths of:
 *      4:3   16:9
 * ---------------
 * PAL  1396  1862
 * NTSC 1164  1552
 *
 * BufferWidth  MAGH+1 (old)      MAGH+1 (new) x1.6  x2    x2.4  x3
 * ------------------------------------------------------------------
 * 512          *  5 / 4 = 640    * 2 =        1024,       1536
 * 640          *  4 / 4 = 640    * 2 =              1280,       1920
 * 
 */
static u64 mod_DISPLAY(struct gsm_state *pstate, u64 r)
{
    int dx   = (r      ) & 0xfff;
    int dy   = (r >> 12) & 0x7ff;
    int magh = (r >> 23) & 0x00f;
    int magv = (r >> 27) & 0x003;
    int dw   = (r >> 32) & 0xfff;
    int dh   = (r >> 44) & 0x7ff;

    // Normalize values
    magh += 1;
    magv += 1;
    dw   += 1;
    dh   += 1;

    // Change width
    int magh_new = magh * pstate->MAGHMul / pstate->VCKDiv;
    int dw_new   = dw * magh_new / magh;
    int dx_new   = pstate->gsm.x_center_vck - (dw_new / 2);

    // Change height
    int magv_new = magv * pstate->FBMul / pstate->FBDiv;
    int dh_new   = dh   * pstate->DPMul / pstate->DPDiv;
    int dy_new   = pstate->gsm.y_center - (dh_new / 2);

    (void)dx; // Unused
    (void)dy; // Unused

    // Add game offset
    //int dx_off_vck = dx - (pstate->game.x_center_vck - (dw / 2));
    //int dy_off     = dy - (pstate->game.y_center     - (dh / 2));
    //dx_new += dx_off_vck * magh_new / magh;
    //dy_new += dy_off     * magv_new / magv;

    // Do not change even/odd order
    //dy_new = (dy_new & ~1) | (dy & 1);

    // Registers are zero-based
    magh_new -= 1;
    magv_new -= 1;
    dw_new   -= 1;
    dh_new   -= 1;

    return GS_SET_DISPLAY(dx_new, dy_new, magh_new, magv_new, dw_new, dh_new);
}

// mode, interlace and ffmd must be set before calling this function
static void update_scaling_center(struct video_mode *mode)
{
    int display_width;  // pixel units
    int display_height; // pixel units
    int display_xoff;   // pixel units
    int display_yoff;   // pixel units

    switch(mode->mode) {
        case 0x02: // 480i
        case 0x50: // 480p
            display_width  = 720;
            display_height = 480;
            display_xoff   = 123;
            display_yoff   =  34;
            mode->vck      = (mode->mode == 0x02) ? 4 : 2;
            mode->FBHeight = 2; // 480
            mode->DPHeight = 2; // 480
            break;
        case 0x03: // 576i
        case 0x53: // 576p
            display_width  = 720;
            display_height = 576;
            display_xoff   = 130;
            display_yoff   =  40;
            mode->vck      = (mode->mode == 0x03) ? 4 : 2;
            mode->FBHeight = 2; // 480
            mode->DPHeight = 2; // 480
            break;
        case 0x51: // 1080i
            display_width  = 1920;
            display_height = 1080;
            display_xoff   =  236;
            display_yoff   =   38;
            mode->vck      =    1;
            mode->FBHeight = 4; // 960
            mode->DPHeight = 4; // 960
            break;
        default:
            // Unsupported mode
            return;
    }

    if (mode->interlace == 1 && mode->ffmd == 1) {
        // interlaced FRAME mode uses half height framebuffer
        // 960 -> 480
        // 480 -> 240
        mode->FBHeight /= 2;
    }

    if ((mode->mode == 0x02 || mode->mode == 0x03) && mode->interlace == 0) {
        // non-interlaced PAL/NTSC video mode is only half the height
        mode->FBHeight /= 2; // 480 -> 240
        mode->DPHeight /= 2; // 480 -> 240
        display_height /= 2; // 480 -> 240
        display_yoff   /= 2;
    }

    mode->x_center_vck = (display_xoff + (display_width  / 2)) * mode->vck;
    mode->y_center     =  display_yoff + (display_height / 2);
}

static void update_scaling(struct gsm_state *pstate)
{
    // Get center positions
    update_scaling_center(&pstate->game);
    update_scaling_center(&pstate->gsm);

    // Multiplication factors assume the mimimum video mode of 240p/288p or 480i/576i FRAME mode:
    //   WidthMul  x1 = 640
    //   HeightMul x1 = 224 (240p) / 256 (288p)
    if (pstate->GsmVideoMode == EECORE_GSM_VMODE_FP1) {
        // 240p/288p mode
        pstate->MAGHMul = 1; // 640 * 1 = 640
    } else if (pstate->GsmVideoMode == EECORE_GSM_VMODE_FP2) {
        // 480p/576p mode
        pstate->MAGHMul = 1; // 640 * 1 = 640
    } else if (pstate->GsmVideoMode == EECORE_GSM_VMODE_1080I_X1) {
        // 1080i mode
        pstate->MAGHMul = 1; // 640 * 1 = 640
        pstate->gsm.FBHeight /= 2;
        pstate->gsm.DPHeight /= 2;
    } else if (pstate->GsmVideoMode == EECORE_GSM_VMODE_1080I_X2) {
        // 1080i mode
        pstate->MAGHMul = 2; // 640 * 2 = 1280
    } else if (pstate->GsmVideoMode == EECORE_GSM_VMODE_1080I_X3) {
        // 1080i mode
        pstate->MAGHMul = 3; // 640 * 3 = 1920
    }

    // How much larger/smaller is the framebuffer
    if (pstate->game.FBHeight > pstate->gsm.FBHeight) {
        pstate->FBMul = 1;
        pstate->FBDiv = pstate->game.FBHeight / pstate->gsm.FBHeight;
    } else {
        pstate->FBMul = pstate->gsm.FBHeight / pstate->game.FBHeight;
        pstate->FBDiv = 1;
    }
    // How much larger/smaller is the display
    if (pstate->game.DPHeight > pstate->gsm.DPHeight) {
        pstate->DPMul = 1;
        pstate->DPDiv = pstate->game.DPHeight / pstate->gsm.DPHeight;
    } else {
        pstate->DPMul = pstate->gsm.DPHeight / pstate->game.DPHeight;
        pstate->DPDiv = 1;
    }
    pstate->VCKDiv = pstate->game.vck / pstate->gsm.vck;
}

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
        _ee_disable_bpc();
        BGERROR(COLOR_FUNC_GSM, 1);
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
    u64 *dest = NULL;
    u64 *source = NULL;
    u64 value = 0;
    if (op == MOP_SD) {
        value  = (u64)regs->gpr[rt];
        dest   = (u64*)((u32)regs->gpr[rs] + imm);
    } else if (op == MOP_LD) {
        source = (u64*)((u32)regs->gpr[rs] + imm);
    } else {
        _ee_disable_bpc();
        BGERROR(COLOR_FUNC_GSM, 2);
        return;
    }

    // Disable breakpoint while handling GS registers
    u32 bpc_save = _ee_disable_bpc();

    // Process LD instruction (read from register)
    switch((u32)source & 0x1fffffff) {
        case (u32)GS_REG_CSR:
            // Emulate field flipping
            u64 csr = *source;
            u32 VSINT = (csr >> 3) & 1;
            u32 FIELD_emu = (csr >> 13) & 1;

            if (pstate->GsmCompMode == EECORE_GSM_COMP_1) {
                //
                // mode 1: FIELD flips at VSYNC interrupt with forced ACK
                //
                if (VSINT == 1) {
                    pstate->VSINTCount++;
                    // ACK the VSYNC interrupt
                    // Normally the game would do this!
                    *GS_REG_CSR = (1 << 3);
                }
                FIELD_emu = pstate->VSINTCount & 1;
            } else if (pstate->GsmCompMode == EECORE_GSM_COMP_2) {
                //
                // mode 2: FIELD flips at VSYNC interrupt rising edge
                //
                if (VSINT == 1 && pstate->VSINTPrev == 0) {
                    pstate->VSINTCount++;
                }
                FIELD_emu = pstate->VSINTCount & 1;
            } else if (pstate->GsmCompMode == EECORE_GSM_COMP_3) {
                //
                // mode 3: invert FIELD
                //
                //FIELD_emu = !FIELD_emu;
            }

            // Insert emulated FIELD bit
            csr = (csr & ~(1<<13)) | FIELD_emu << 13;
            // Store old value
            pstate->VSINTPrev = VSINT;
            // Return emulated CSR
            regs->gpr[rt] = csr;
            break;
        case 0:
            break;
        case (u32)GS_REG_PMODE:
        case (u32)GS_REG_SMODE2:
        case (u32)GS_REG_DISPLAY1:
        case (u32)GS_REG_DISPLAY2:
        case (u32)GS_REG_SIGLBLID:
            regs->gpr[rt] = *source;
            break;
        default:
            BGERROR(COLOR_FUNC_GSM, 3);
    }

    // Process SD instruction (write to register)
    switch((u32)dest & 0x1fffffff) {
        case (u32)GS_REG_DISPLAY1:
            pstate->last_display1 = value;
            *dest = mod_DISPLAY(pstate, value);
            break;
        case (u32)GS_REG_DISPLAY2:
            pstate->last_display2 = value;
            *dest = mod_DISPLAY(pstate, value);
            break;
        case (u32)GS_REG_SMODE2:
            // Store game requested mode
            pstate->game.interlace = (value     ) & 1; // Switching modes is only possible using SetGsCrt ???
            pstate->game.ffmd      = (value >> 1) & 1;
            if (pstate->game.interlace == 1) {
                pstate->gsm.ffmd = pstate->game.ffmd;
            }
            update_scaling(pstate);
            // Re-process last DISPLAY register writes
            if (pstate->last_display1 != 0)
                *GS_REG_DISPLAY1 = mod_DISPLAY(pstate, pstate->last_display1);
            if (pstate->last_display2 != 0)
                *GS_REG_DISPLAY2 = mod_DISPLAY(pstate, pstate->last_display2);

            *dest = GS_SET_SMODE2(pstate->gsm.interlace, pstate->gsm.ffmd, 0);
            break;
        case (u32)GS_REG_CSR:
            // Detect ACK of VSINT interrupt
            if (value & (1 << 3))
                pstate->VSINTPrev = 0;
            *dest = value;
            break;
        case 0:
            break;
        case (u32)GS_REG_PMODE:
        case (u32)GS_REG_SIGLBLID:
            *dest = value;
            break;
        default:
            BGERROR(COLOR_FUNC_GSM, 4);
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
                        _ee_disable_bpc();
                        BGERROR(COLOR_FUNC_GSM, 5);
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
                        _ee_disable_bpc();
                        BGERROR(COLOR_FUNC_GSM, 6);
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
                _ee_disable_bpc();
                BGERROR(COLOR_FUNC_GSM, 7);
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

    // Set game state
    pstate->game.interlace = interlace;
    pstate->game.ffmd      = ffmd;
    pstate->game.mode      = mode;

    // Set GSM state (default game state)
    pstate->gsm = pstate->game;

    // Only NTSC(2) and PAL(3) modes are supported
    if ((pstate->game.mode != 2) && (pstate->game.mode != 3))
        return;
    
    // Set GSM state
    if (pstate->GsmVideoMode == EECORE_GSM_VMODE_FP1) {
        // 240p/288p non-interlaced FRAME mode -> no change
        // 480i/576i     interlaced FRAME mode -> 240p/288p (can look ugly if it was true interlaced)
        // 480i/576i     interlaced FIELD mode -> no change
        if (pstate->game.interlace == 1 && pstate->game.ffmd == 1) {
            pstate->gsm.interlace = 0; // non-interlaced
            pstate->gsm.ffmd      = 1; // FRAME mode
        }
    } else if (pstate->GsmVideoMode == EECORE_GSM_VMODE_FP2) {
        // 576p not supported on pre-DECKARD consoles
        if (pstate->game.mode == 3 && (pstate->flags & EECORE_FLAG_GSM_NO_576P))
            return;

        // 240p/288p non-interlaced FRAME mode -> 480p/576p (line-double)
        // 480i/576i     interlaced FRAME mode -> 480p/576p (line-double, can look ugly if it was true interlaced)
        // 480i/576i     interlaced FIELD mode -> 480p/576p (show all lines! output quality x2!)
        pstate->gsm.interlace = 0; // non-interlaced
        pstate->gsm.ffmd      = 1; // FRAME mode
        if (pstate->game.mode == 2) {
            pstate->gsm.mode = 0x50; // 480p
        } else {
            pstate->gsm.mode = 0x53; // 576p
        }
    } else if (pstate->GsmVideoMode == EECORE_GSM_VMODE_1080I_X1 ||
               pstate->GsmVideoMode == EECORE_GSM_VMODE_1080I_X2 ||
               pstate->GsmVideoMode == EECORE_GSM_VMODE_1080I_X3) {
        // 240p/288p non-interlaced FRAME mode -> 1080i FRAME (line 4x)
        // 480i/576i     interlaced FRAME mode -> 1080i FRAME (line 4x, can look ugly if it was true interlaced)
        // 480i/576i     interlaced FIELD mode -> 1080i FIELD (line 2x, output quality x2!)
        pstate->gsm.interlace = 1; // interlaced
        pstate->gsm.ffmd      = pstate->game.ffmd; // keep game setting
        pstate->gsm.mode      = 0x51; // 1080i
    }

    update_scaling(pstate);

    // Set GSM video mode (without triggering a breakpoint)
    _ee_disable_bpc();
    pstate->org_SetGsCrt(pstate->gsm.interlace, pstate->gsm.mode, pstate->gsm.ffmd);

    // Enable breakpoints
    if (pstate->GsmCompMode != EECORE_GSM_COMP_NONE) {
        // For FIELD flipping we need to also set a breakpoint for reading
        _ee_enable_bpc(EE_BPC_DRE | EE_BPC_DWE | EE_BPC_DUE | EE_BPC_DKE);
    } else {
        _ee_enable_bpc(EE_BPC_DWE | EE_BPC_DUE | EE_BPC_DKE);
    }
}

void EnableGSM(void)
{
    struct gsm_state *pstate = &state;
    vu32 *v_debug = (vu32 *)0x80000100;

    pstate->GsmVideoMode = eec.GsmVideoMode;
    pstate->GsmCompMode  = eec.GsmCompMode;
    pstate->flags        = eec.flags;

    // Hook SetGsCrt
    pstate->org_SetGsCrt = GetSyscallHandler(__NR_SetGsCrt);
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
    //   0x12000000 = GS_REG_PMODE    (unwanted)
    //   0x12000020 = GS_REG_SMODE2   (write only)
    //   0x12000080 = GS_REG_DISPLAY1 (write only)
    //   0x120000a0 = GS_REG_DISPLAY2 (write only)
    //   0x12001000 = GS_REG_CSR      (read / write) - only when FIELD flipping
    //   0x12001080 = GS_REG_SIGLBLID (unwanted)     - only when FIELD flipping
    //   0x1fffff5f = mask
    //   0x1fffef5f = mask + CSR
    _ee_mtdab(0x12000000);
    if (eec.GsmCompMode != EECORE_GSM_COMP_NONE) {
        // For FIELD flipping we need to also set a breakpoint for CSR register
        _ee_mtdabm(0x1fffef5f);
    } else {
        _ee_mtdabm(0x1fffff5f);
    }
}

void DisableGSM(void)
{
    struct gsm_state *pstate = &state;

    // Make sure no exceptions are generated
    _ee_disable_bpc();
    // Restore syscalls
    SetSyscall(__NR_SetGsCrt, pstate->org_SetGsCrt);
}
