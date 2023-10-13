#include <loadcore.h>
#include <usbd.h>
#include "mprintf.h"

#define MODNAME "usbd"
IRX_ID(MODNAME, 1, 1);

extern struct irx_export_table _exp_usbd;

#ifdef DEBUG
int sceUsbdRegisterLdd(sceUsbdLddOps *driver)
{
    M_DEBUG("%s\n", __FUNCTION__);
    return 0;
}

int sceUsbdUnregisterLdd(sceUsbdLddOps *driver)
{
    M_DEBUG("%s\n", __FUNCTION__);
    return 0;
}

void *sceUsbdScanStaticDescriptor(int devId, void *data, u8 type)
{
    M_DEBUG("%s\n", __FUNCTION__);
    return NULL;
}

int sceUsbdSetPrivateData(int devId, void *data)
{
    M_DEBUG("%s\n", __FUNCTION__);
    return 0;
}

void *sceUsbdGetPrivateData(int devId)
{
    M_DEBUG("%s\n", __FUNCTION__);
    return NULL;
}

int sceUsbdOpenPipe(int devId, UsbEndpointDescriptor *desc)
{
    M_DEBUG("%s\n", __FUNCTION__);
    return 0;
}

int sceUsbdClosePipe(int id)
{
    M_DEBUG("%s\n", __FUNCTION__);
    return 0;
}

int sceUsbdTransferPipe(int id, void *data, u32 len, void *option, sceUsbdDoneCallback callback, void *cbArg)
{
    M_DEBUG("%s\n", __FUNCTION__);
    return 0;
}

int sceUsbdOpenPipeAligned(int devId, UsbEndpointDescriptor *desc)
{
    M_DEBUG("%s\n", __FUNCTION__);
    return 0;
}

int sceUsbdGetDeviceLocation(int devId, u8 *path)
{
    M_DEBUG("%s\n", __FUNCTION__);
    return 0;
}

int sceUsbdRegisterAutoloader(sceUsbdLddOps *drv)
{
    M_DEBUG("%s\n", __FUNCTION__);
    return 0;
}

int sceUsbdUnregisterAutoloader(void)
{
    M_DEBUG("%s\n", __FUNCTION__);
    return 0;
}

int sceUsbdChangeThreadPriority(int prio1, int prio2)
{
    M_DEBUG("%s\n", __FUNCTION__);
    return 0;
}
#endif

int _start(int argc, char **argv)
{
    M_DEBUG("%s\n", __FUNCTION__);

    if (RegisterLibraryEntries(&_exp_usbd) != 0) {
        M_DEBUG("%s: failed to register\n", __FUNCTION__);
        return MODULE_NO_RESIDENT_END;
    }

    return MODULE_RESIDENT_END;
}
