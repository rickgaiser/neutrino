#include <loadcore.h>
#include <sysmem.h>
#include <tamtypes.h>

#include "mprintf.h"
#include "ioplib.h"

#define MODNAME "freemem"
IRX_ID(MODNAME, 1, 1);

typedef void * (*fp_AllocSysMemory)(int mode, int size, void *ptr);
typedef u32    (*fp_QueryMaxFreeMemSize)();
fp_AllocSysMemory        org_AllocSysMemory;
fp_QueryMaxFreeMemSize   org_QueryMaxFreeMemSize;
static u32 limit;
static u32 block;

static void * hooked_AllocSysMemory(int mode, int size, void *ptr)
{
    M_DEBUG("%s(%d, %dB/%dKiB, 0x%x)\n", __FUNCTION__, mode, size, size/1024, ptr);

    if (size == block) {
        M_DEBUG("-\n- HACK! denying requested memory!\n-\n");
        return NULL;
    }

    return org_AllocSysMemory(mode, size, ptr);
}

static u32 hooked_QueryMaxFreeMemSize()
{
    u32 rv = org_QueryMaxFreeMemSize();

    M_DEBUG("%s() = %d (%dKiB) -> HACK! limit to %dKiB!\n", __FUNCTION__, rv, rv/1024, limit/1024);
    if (rv > limit)
        rv = limit;

    return rv;
}

int _start(int argc, char **argv)
{
    int i;

    // Set defaults
    limit = 512; // Default 512KiB limit
    block = 0;   // Default no blocking

    // Parse arguments
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == 'X') {
            // Block max memory requests
            block = 1;
        } else {
            // Size in KiB
            // Note that there is NO checking for valid characters here!
            char *c = argv[i];

            limit = 0;
            while (*c != 0) {
                limit *= 10;
                limit += (*c - '0');
                c++;
            }
        }
    }
    limit *= 1024; // KiB to B
    if (block != 0)
        block = limit;

    // Hook sysmem functions
    iop_library_t * lib_sysmem = ioplib_getByName("sysmem");
    if (block != 0)
        org_AllocSysMemory = ioplib_hookExportEntry(lib_sysmem, 4, hooked_AllocSysMemory);
    org_QueryMaxFreeMemSize = ioplib_hookExportEntry(lib_sysmem, 7, hooked_QueryMaxFreeMemSize);
    ioplib_relinkExports(lib_sysmem);

    return MODULE_RESIDENT_END;
}
