#include <loadcore.h>
#include <sysmem.h>
#include <tamtypes.h>

#include "mprintf.h"
#include "ioplib.h"

#define MODNAME "rc_uya"
IRX_ID(MODNAME, 1, 1);

typedef void * (*fp_AllocSysMemory)(int mode, int size, void *ptr);
fp_AllocSysMemory org_AllocSysMemory;

static void * hooked_AllocSysMemory(int mode, int size, void *ptr)
{
    M_DEBUG("%s(%d, %d, 0x%x), free=%dKiB\n", __FUNCTION__, mode, size, ptr, QueryTotalFreeMemSize()/1024);

    // RC3 / UYA - ONLINE!
    //
    // This game seems to assume a 321792 size buffer is always located at 0x4c900.
    //
    // This buffer is probably allocated from the EE side, and used to load
    // IOP modules. There seems to be a 'bug' in this game that assumes
    // this memory will always be located at 0x4c900.

    if (size == 321792) {
        M_DEBUG("-\n- HACK! forcing requested memory location to 0x4c900\n-\n");
        return org_AllocSysMemory(ALLOC_ADDRESS, size, (void *)0x4c900);
    }
    else {
        return org_AllocSysMemory(mode, size, ptr);
    }
}

int _start(int argc, char **argv)
{
    // hook AllocSysMemory
    iop_library_t * lib_sysmem = ioplib_getByName("sysmem");
    org_AllocSysMemory = ioplib_hookExportEntry(lib_sysmem, 4, hooked_AllocSysMemory);
    ioplib_relinkExports(lib_sysmem);

    return MODULE_RESIDENT_END;
}
