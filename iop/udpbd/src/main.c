#include <irx.h>
#include <loadcore.h>

#include "main.h"
#include "udpbd.h"

IRX_ID(MODNAME, 0x1, 0x0);

int _start(int argc, char *argv[])
{
    (void)argc; (void)argv;
    udpbd_init();
    return MODULE_RESIDENT_END;
}
