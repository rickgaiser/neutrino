/*
  Copyright 2010, jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.

  The goal of the code in this file is to force syscall hooks to use
  an alternate stack when they need to use stack.
*/

#include <ee_cop0_defs.h>
#include <syscallnr.h>

#define ABI_EABI64 // force all register names to EABI64 (legacy toolchain)
#include "as_reg_compat.h"

#include "ee_core_compat.h"

.set push
.set noreorder
.set noat

/**************************************************************************
 *
 * .text section
 *
 **************************************************************************/

.text

.extern _stack_end

/* libkernel */
.extern memset
.extern memcpy
.extern strlen
.extern strncpy
.extern ExecPS2

/* syshook.c */
.extern g_argc
.extern g_argv
.extern set_reg_hook
.extern iop_reboot_count
.extern New_SifSetDma
.extern Old_SifSetDma
.extern Old_SifSetReg
.extern Old_SifGetReg
.extern Old_ExecPS2
.extern Old_CreateThread
.extern sysExit

/* loader.h */
.extern g_compat_mask
.extern EnableDebug

.globl  g_argbuf
.globl  Hook_SifSetDma
.globl  Hook_SifSetReg
.globl  Hook_Exit
.globl  CleanExecPS2

/*
 * _SifSetDma: function designed to use our own stack during IOP reboot trap
 */
 .ent    _SifSetDma
_SifSetDma:

    /* save original stack pointer */
    daddu   $a2, $sp, $zero

    /* change the stack pointer */
    la      $sp, _stack_end

    /* call New_SifSetDma, preserving ra and a2 registers values */
    addiu   $sp, $sp, -0x10
    sd      $ra, 0x0000($sp)
    jal     New_SifSetDma
    sd      $a2, 0x0008($sp)

    /* restore sp and ra registers */
    ld      $ra, 0x0000($sp)
    ld      $sp, 0x0008($sp)

    jr      $ra
    addiu   $v0, $zero, 1

.end    _SifSetDma

/*
 * Hook_SifSetDma: exit syscall to _SifSetDma when IOP reboot trapped
 */
 .ent    Hook_SifSetDma
Hook_SifSetDma:

    /* check ((SifDmaTransfer_t *)$a0)->attr == 0x44 */
    lw      $v1, 0x000c($a0)
    addiu   $v0, $zero, 0x44
    bne     $v0, $v1, 2f

    /* check ((SifDmaTransfer_t *)$a0)->size == 0x68 */
    lw      $v1, 0x0008($a0)
    addiu   $v0, $zero, 0x68
    beq     $v0, $v1, 1f

    /* check ((SifDmaTransfer_t *)$a0)->size == 0x70 */
    addiu   $v0, $zero, 0x70
    bne     $v0, $v1, 2f
1:
    /* check (SifCmdResetData *)((SifDmaTransfer_t *)$a0->src)->chdr.psize == ((SifDmaTransfer_t *)$a0)->size */
    lw      $a2, 0x0000($a0)
    lw      $v0, 0x0000($a2)
    bne     $v0, $v1, 2f

    /* check (SifCmdResetData *)((SifDmaTransfer_t *)$a0->src)->chdr.fcode == 0x80000003 */
    lui     $a3, 0x8000
    ori     $a3, $a3, 0x0003
    lw      $v0, 0x0008($a2)
    bne     $v0, $a3, 2f
    nop

    /* exit syscall to _SifSetDma */
    la      $v1, _SifSetDma
    jr      $ra
    sw      $v1, 0x0008($sp)
2:
    /* call & return with original SifSetDma */
    lw      $v0, Old_SifSetDma
    jr      $v0
    nop

.end    Hook_SifSetDma

/*
 * _Apply_Mode3: unhook SifSetDma/SifSetReg
 */
.ent    _Apply_Mode3
_Apply_Mode3:

    /* save original stack pointer */
    daddu   $a0, $sp, $zero

    /* change the stack pointer */
    la      $sp, _stack_end

    /* preserving ra and a0 registers values */
    addiu   $sp, $sp, -0x10
    sd      $ra, 0x0000($sp)
    sd      $a0, 0x0008($sp)

    /* unhook SifSetDma */
    addiu   $a0, $zero, __NR_SifSetDma
    lw      $a1, Old_SifSetDma
    addiu   $v1, $zero, __NR_SetSyscall
    syscall

    /* unhook SifSetReg */
    addiu   $a0, $zero, __NR_SifSetReg
    lw      $a1, Old_SifSetReg
    addiu   $v1, $zero, __NR_SetSyscall
    syscall

    /* unhook SifGetReg */
    addiu   $a0, $zero, __NR_SifGetReg
    lw      $a1, Old_SifGetReg
    addiu   $v1, $zero, __NR_SetSyscall
    syscall

    /* FlushCache */
    daddu   $a0, $zero, $zero
    addiu   $v1, $zero, __NR_FlushCache
    syscall
    addiu   $a0, $zero, 2
    addiu   $v1, $zero, __NR_FlushCache
    syscall

    /* restore sp and ra registers */
    ld      $ra, 0x0000($sp)
    ld      $sp, 0x0008($sp)

    jr      $ra
    addiu   $v0, $zero, 1

.end    _Apply_Mode3

/*
 * Hook_SifSetReg: disable SifSetReg and unhook SifSetDma/SifSetReg when needed
 */
.ent    Hook_SifSetReg
Hook_SifSetReg:

    /* load set_reg_hook counter to a2 */
    la      $a2, set_reg_hook
    lw      $v0, 0x0000($a2)

    /* check set_reg_hook is != 0, otherwise execute normal SifSetReg */
    bne     $v0, $zero, 1f
    addiu   $v0, $v0, -1
    lw      $v0, Old_SifSetReg
    jr      $v0
    nop
1:
    /* decrement set_reg_hook counter by 1 */
    sw      $v0, 0x0000($a2)
    bne     $v0, $zero, 3f
    nop

    lw      $v0, EnableDebug
    bne     $v0, $zero, 2f
    nop

    /* black BG color */
    lui     $v0, 0x1200
    ori     $v0, $v0, 0x00e0
    sd      $zero, 0x0000($v0)
2:
    /* check compat mode 3 is enabled */
    lw      $v0, g_compat_mask
    andi    $v0, $v0, EECORE_COMPAT_UNHOOK
    beq     $v0, $zero, 3f

    /* check iop_reboot_count is == 2 */
    addiu   $v1, $zero, 2
    lw      $v0, iop_reboot_count
    bne     $v0, $v1, 3f
    nop

    /* exit syscall to _Apply_Mode3 */
    la      $v1, _Apply_Mode3
    sw      $v1, 0x0008($sp)
3:
    jr      $ra
    addiu   $v0, $zero, 1

.end    Hook_SifSetReg

.ent    Hook_Exit
Hook_Exit:
    la      $v1, sysExit
    jr      $ra
    sw      $v1, 0x0008($sp)

.end    Hook_Exit

.ent CleanExecPS2
CleanExecPS2:
    jal     InitRegsExecPS2
    nop

    j       ExecPS2
    por     $ra, $zero, $zero

.end CleanExecPS2

.ent InitRegsExecPS2
InitRegsExecPS2:
    /* Erase all registers, so that all games will have the same boot state regardless of whatever OPL was doing.
       Note: Sony added something like this at the start of crt0 for newer SDK releases. But older games will still not have this.
       While this is not as clean, it is the best thing to do without changing the game. */
    por     $at, $zero, $zero
    por     $v0, $zero, $zero
    por     $v1, $zero, $zero
    # por     $a0, $zero, $zero     # Used for entrypoint
    # por     $a1, $zero, $zero     # Used for gp
    # por     $a2, $zero, $zero     # Used for argc
    # por     $a3, $zero, $zero     # Used for argv
    por     $t0, $zero, $zero
    por     $t1, $zero, $zero
    por     $t2, $zero, $zero
    por     $t3, $zero, $zero
    por     $t4, $zero, $zero
    por     $t5, $zero, $zero
    por     $t6, $zero, $zero
    por     $t7, $zero, $zero
    por     $s0, $zero, $zero
    por     $s1, $zero, $zero
    por     $s2, $zero, $zero
    por     $s3, $zero, $zero
    por     $s4, $zero, $zero
    por     $s5, $zero, $zero
    por     $s6, $zero, $zero
    por     $s7, $zero, $zero
    por     $t8, $zero, $zero
    por     $t9, $zero, $zero
    por     $gp, $zero, $zero
    la      $sp, _stack_end         # Reset SP to top of stack
    por     $fp, $zero, $zero
    # por     $ra, $zero, $zero       # Required for returning
    mthi    $zero
    mthi1   $zero
    mtlo    $zero
    mtlo1   $zero
    mtsah   $zero, 0
    mtc1    $zero, $f0
    mtc1    $zero, $f1
    mtc1    $zero, $f2
    mtc1    $zero, $f3
    mtc1    $zero, $f4
    mtc1    $zero, $f5
    mtc1    $zero, $f6
    mtc1    $zero, $f7
    mtc1    $zero, $f8
    mtc1    $zero, $f9
    mtc1    $zero, $f10
    mtc1    $zero, $f11
    mtc1    $zero, $f12
    mtc1    $zero, $f13
    mtc1    $zero, $f14
    mtc1    $zero, $f15
    mtc1    $zero, $f16
    mtc1    $zero, $f17
    mtc1    $zero, $f18
    mtc1    $zero, $f19
    mtc1    $zero, $f20
    mtc1    $zero, $f21
    mtc1    $zero, $f22
    mtc1    $zero, $f23
    mtc1    $zero, $f24
    mtc1    $zero, $f25
    mtc1    $zero, $f26
    mtc1    $zero, $f27
    mtc1    $zero, $f28
    mtc1    $zero, $f29
    mtc1    $zero, $f30
    mtc1    $zero, $f31
    adda.s  $f0, $f1             # Clear facc
    sync.p

    jr      $ra
    ctc1    $zero, $f31
.end InitRegsExecPS2

.set pop
