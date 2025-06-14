#include <loadcore.h>
#include <sysmem.h>
#include <tamtypes.h>

#include "ioplib.h"

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

static void * hooked_AllocSysMemory(int mode, int size, void *ptr)
{
    return org_AllocSysMemory(mode, size + extra_size, ptr);
}

int _start(int argc, char **argv)
{
    if (argc >= 1) {
        char c = argv[1][0];
        if (c >= '0' && c <= '9')
            extra_size = 256 << (c - '0');
    }

    // Hook sysmem functions
    iop_library_t * lib_sysmem = ioplib_getByName("sysmem");
    org_AllocSysMemory = ioplib_hookExportEntry(lib_sysmem, 4, hooked_AllocSysMemory);
    ioplib_relinkExports(lib_sysmem);

    return MODULE_RESIDENT_END;
}
