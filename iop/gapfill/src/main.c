#include <loadcore.h>
#include <sysmem.h>
#include <tamtypes.h>

#include "mprintf.h"

#define MODNAME "gapfill"
IRX_ID(MODNAME, 1, 1);

int _start(int argc, char **argv)
{
    //M_DEBUG("Total free: %dB/%dKiB\n", QueryTotalFreeMemSize(), QueryTotalFreeMemSize() / 1024);

    void *ptrs[10];
    int idx=0;
    u32 size;

    while ((size = QueryMaxFreeMemSize()) > 0) {
        M_DEBUG("- Max free: %dB/%dKiB\n", QueryMaxFreeMemSize(), QueryMaxFreeMemSize() / 1024);
        ptrs[idx] = AllocSysMemory(0, size, NULL);

        // Free only large blocks, "memory leak" the small blocks
        if (size > (16*10242))
            idx++;
    }

    int i;
    for (i=0; i<idx; i++) {
        FreeSysMemory(ptrs[i]);
    }

    return MODULE_NO_RESIDENT_END;
}
