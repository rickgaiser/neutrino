#ifndef EE_REGS_H
#define EE_REGS_H


#include <tamtypes.h>


/*
 * Struct containing 'all' registers needed to save/restore the program state
 * Note that we are not saving FP and COP0 registers, so keep them unchanged.
 */
typedef struct ee_registers
{
    // 32 general purpose registers
    union {
        u128 gpr[32];
        struct {
            // EABI64 register names
            u128 zero;
            u128 at;
            u128 v0;
            u128 v1;
            u128 a0;
            u128 a1;
            u128 a2;
            u128 a3;
            u128 t0;
            u128 t1;
            u128 t2;
            u128 t3;
            u128 t4;
            u128 t5;
            u128 t6;
            u128 t7;
            u128 s0;
            u128 s1;
            u128 s2;
            u128 s3;
            u128 s4;
            u128 s5;
            u128 s6;
            u128 s7;
            u128 t8;
            u128 t9;
            u128 k0;
            u128 k1;
            u128 gp;
            u128 sp;
            u128 fp;
            u128 ra;
        };
    };

    u128 hi;
    u128 lo;
} ee_registers_t;


#endif
