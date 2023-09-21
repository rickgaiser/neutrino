// libc/newlib
#include <string.h>

// PS2SDK
#include <kernel.h>
#include <stdint.h>


/*  Returns the patch location of LoadExecPS2(), which resides in kernel memory.
 *  Patches the kernel to use the EELOAD module at the specified location.
 *  Must be run in kernel mode.
 */
void *sbvpp_replace_eeload(void *new_eeload)
{
    void *result = NULL;
    uint32_t *p;

    /* The pattern of the code in LoadExecPS2() that prepares the kernel for copying EELOAD from rom0: */
    static const unsigned int initEELOADCopyPattern[] = {
        0x8FA30010, /* lw       v1, 0x0010(sp) */
        0x0240302D, /* daddu    a2, s2, zero */
        0x8FA50014, /* lw       a1, 0x0014(sp) */
        0x8C67000C, /* lw       a3, 0x000C(v1) */
        0x18E00009, /* blez     a3, +9 <- The kernel will skip the EELOAD copying loop if the value in $a3 is less than, or equal to 0. Lets do that... */
    };

    DI();
    ee_kmode_enter();

    /* Find the part of LoadExecPS2() that initilizes the EELOAD copying loop's variables */
    for (p = (uint32_t *)0x80001000; p < (uint32_t *)0x80030000; p++) {
        if (memcmp(p, &initEELOADCopyPattern, sizeof(initEELOADCopyPattern)) == 0) {
            p[1] = 0x3C120000 | (uint16_t)((uint32_t)new_eeload >> 16);    /* lui s2, HI16(new_eeload) */
            p[2] = 0x36520000 | (uint16_t)((uint32_t)new_eeload & 0xFFFF); /* ori s2, s2, LO16(new_eeload) */
            p[3] = 0x24070000;                                             /* li a3, 0 <- Disable the EELOAD copying loop */
            result = (void *)p;
            break; /* All done. */
        }
    }

    ee_kmode_exit();
    EI();

    return result;
}

void *sbvpp_patch_user_mem_clear(void *start)
{
    void *ret = NULL;
    uint32_t *p;

    DI();
    ee_kmode_enter();

    for (p = (uint32_t *)0x80001000; p < (uint32_t *)0x80080000; p++) {
        /*
         * Search for function call and patch $a0
         *  lui  $a0, 0x0008
         *  jal  InitializeUserMemory
         *  ori  $a0, $a0, 0x2000
         */
        if (p[0] == 0x3c040008 && (p[1] & 0xfc000000) == 0x0c000000 && p[2] == 0x34842000) {
            p[0] = 0x3c040000 | ((uint32_t)start >> 16);
            p[2] = 0x34840000 | ((uint32_t)start & 0xffff);
            ret = (void *)p;
            break;
        }
    }

    ee_kmode_exit();
    EI();

    return ret;
}
