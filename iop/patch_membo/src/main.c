#include <loadcore.h>
#include <sysmem.h>
#include <tamtypes.h>

#include "ioplib.h"
#include "mprintf.h"

#define MODNAME "pmembo"
IRX_ID(MODNAME, 1, 1);

/*
 * membo = Memory Buffer Overrun
 * -----------------------------
 * This module tries to fix games that have a buffer overrun issue.
 * By allocating 256 bytes (the smallest amount) or more extra.
 */

typedef void * (*fp_AllocSysMemory)(int mode, int size, void *ptr);
fp_AllocSysMemory org_AllocSysMemory;
int extra_size = 256;
int gamefix = 0;

static void * hooked_AllocSysMemory(int mode, int size, void *ptr)
{
    void * rv;

    switch(gamefix) {
        case 1:
            // [SLES_524.82] Steel Dragon EX
            if (size == (100*1024)) {size = (256*1024);}
            break;
    }

    rv = org_AllocSysMemory(mode, size + extra_size, ptr);

    M_DEBUG("%s(%d, %dB/%dKiB, 0x%x) = 0x%x\n", __FUNCTION__, mode, size, size/1024, ptr, rv);

    return rv;
}

int _start(int argc, char **argv)
{
    if (argc >= 1) {
        char c = argv[1][0];
        if (c >= '0' && c <= '9')
            extra_size = 256 << (c - '0');
    }

    if (argc >= 2) {
        char c = argv[2][0];
        if (c >= '0' && c <= '9')
            gamefix = (c - '0');
    }

    M_DEBUG("extra_size = %d\n", extra_size);
    M_DEBUG("gamefix    = %d\n", gamefix);

    // Hook sysmem functions
    iop_library_t * lib_sysmem = ioplib_getByName("sysmem");
    org_AllocSysMemory = ioplib_hookExportEntry(lib_sysmem, 4, hooked_AllocSysMemory);
    ioplib_relinkExports(lib_sysmem);

    return MODULE_RESIDENT_END;
}
