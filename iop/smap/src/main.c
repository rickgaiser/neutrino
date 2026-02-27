#include <loadcore.h>
#include <irx.h>

#include "main.h"
#include "xfer.h"

IRX_ID(MODNAME, 0x2, 0x1A);

extern struct irx_export_table _exp_smap __attribute__((section("data")));

int _start(int argc, char *argv[])
{
    int result;

    if ((result = smap_init(argc, argv)) < 0) {
        M_DEBUG("smap: smap_init -> %d\n", result);
        return MODULE_NO_RESIDENT_END;
    }

    if (RegisterLibraryEntries(&_exp_smap) != 0) {
        M_DEBUG("smap: module already loaded\n");
        return MODULE_NO_RESIDENT_END;
    }

    return MODULE_RESIDENT_END;
}
