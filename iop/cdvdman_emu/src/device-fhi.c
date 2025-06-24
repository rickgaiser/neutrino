#include <stdint.h>

#include "internal.h"
#include "device.h"
#include "fhi.h"
#include "ioplib.h"

// FHI exported function pointer
uint32_t (*fp_fhi_size)(int file_handle);
int (*fp_fhi_read)(int file_handle, void *buffer, unsigned int sector_start, unsigned int sector_count);

static uint32_t iso_lsn = 0;

uint32_t fhi_get_lsn()
{
    uint64_t iso_size;
    iop_library_t *lib;

    if (iso_lsn)
        return iso_lsn;

    // Connect to FHI functions
    lib = ioplib_getByName("fhi\0\0\0\0\0");
    fp_fhi_size = lib->exports[4];
    fp_fhi_read = lib->exports[5];

    M_DEBUG("Waiting for device...\n");

    while (1) {
        iso_size = fp_fhi_size(FHI_FID_CDVD);
        if (iso_size > 0)
            break;
        DelayThread(100 * 1000); // 100ms
    }
    iso_lsn = (iso_size + 3) / 4;

    M_DEBUG("Waiting for device...done! connected to %dMiB iso\n", iso_lsn/512);

    return iso_lsn;
}

int fhi_read_sectors(uint32_t vlsn, void *buffer, unsigned int sectors)
{
    int rv = SCECdErNO;
    uint32_t fid = vlsn >> 23;
    uint32_t lsn = vlsn & ((1U << 23) - 1);

    // Late binding
    fhi_get_lsn();

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
