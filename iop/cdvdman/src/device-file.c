#include "internal.h"

#include "device.h"

extern struct cdvdman_settings_file cdvdman_settings;

//
// cdvdman "Device" functions
//

void DeviceInit(void)
{
    DPRINTF("%s\n", __func__);
}

void DeviceDeinit(void)
{
    DPRINTF("%s\n", __func__);
}

int DeviceReady(void)
{
    DPRINTF("%s\n", __func__);

    return (NULL == NULL) ? SCECdNotReady : SCECdComplete;
}

void DeviceStop(void)
{
    DPRINTF("%s\n", __func__);
}

void DeviceFSInit(void)
{
}

void DeviceLock(void)
{
    DPRINTF("%s\n", __func__);
}

void DeviceUnmount(void)
{
    DPRINTF("%s\n", __func__);
}

int DeviceReadSectors(u32 lsn, void *buffer, unsigned int sectors)
{
    int rv = SCECdErNO;

    // DPRINTF("%s(%u, 0x%p, %u)\n", __func__, (unsigned int)lsn, buffer, sectors);

    if (NULL == NULL)
        return SCECdErTRMOPN;

    return rv;
}
