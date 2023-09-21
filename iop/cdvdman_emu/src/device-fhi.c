#include "internal.h"

#include "device.h"
#include "fhi.h"
#include "ioplib.h"

// FHI exported function pointer
uint32_t (*fp_fhi_size)(int file_handle);
int (*fp_fhi_read)(int file_handle, void *buffer, unsigned int sector_start, unsigned int sector_count);
int (*fp_fhi_write)(int file_handle, const void *buffer, unsigned int sector_start, unsigned int sector_count);

void DeviceInit(void)
{
    M_DEBUG("%s\n", __func__);
}

int DeviceReady(void)
{
    //M_DEBUG("%s\n", __func__);

    return SCECdComplete; // SCECdNotReady
}

void DeviceFSInit(void)
{
    uint64_t iso_size;
    iop_library_t *lib;

    // Connect to FHI functions
    lib = ioplib_getByName("fhi\0\0\0\0\0");
    fp_fhi_size = lib->exports[4];
    fp_fhi_read = lib->exports[5];
    fp_fhi_write = lib->exports[6];

    M_DEBUG("Waiting for device...\n");

    while (1) {
        iso_size = fp_fhi_size(FHI_FID_CDVD);
        if (iso_size > 0)
            break;
        DelayThread(100 * 1000); // 100ms
    }

    mediaLsnCount = (iso_size + 3) / 4;

    M_DEBUG("Waiting for device...done! connected to %dMiB iso\n", mediaLsnCount/512);
}

int DeviceReadSectors(u32 vlsn, void *buffer, unsigned int sectors)
{
    int rv = SCECdErNO;
    u32 fid = vlsn >> 23;
    u32 lsn = vlsn & ((1U << 23) - 1);

    // M_DEBUG("%s(%u-%u, 0x%p, %u)\n", __func__, (unsigned int)fid, (unsigned int)lsn, buffer, sectors);

    if (fp_fhi_read(fid, buffer, lsn * 4, sectors * 4) != (sectors * 4)) {
#ifdef DEBUG
        M_DEBUG("%s(%u-%u, 0x%p, %u) FAILED!\n", __func__, (unsigned int)fid, (unsigned int)lsn, buffer, sectors);
        while(1){}
#endif
        rv = SCECdErREAD;
    }

    return rv;
}
