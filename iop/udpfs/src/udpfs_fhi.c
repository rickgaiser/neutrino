/*
 * UDPFS FHI - File Handle Interface over UDPFS/UDPRDMA
 *
 * Minimal FHI implementation for game boot scenarios.
 * Uses pre-opened server-side file handles stored in fhi_fileid struct
 * (populated by EE loader during LE phase via smap_udpfs_ioman.irx).
 *
 * Uses BREAD/BWRITE for single-round-trip sector I/O.
 * Modeled after mmcefhi.
 */

#include <errno.h>
#include <stdint.h>
#include <thbase.h>
#include <thsemap.h>
#include <stdio.h>
#include <loadcore.h>

#include "main.h"
#include "udpfs_core.h"
#include "fhi_fileid.h"


/* Module settings - populated by EE loader via MODULE_SETTINGS_MAGIC scan */
struct fhi_fileid fhi = {MODULE_SETTINGS_MAGIC};

/* Semaphore for thread-safe I/O operations */
static int g_sema = -1;


/*
 * FHI interface: get file size in 512-byte sectors
 */
uint32_t fhi_size(int file_handle)
{
    if (file_handle < 0 || file_handle >= FHI_MAX_FILES)
        return 0;

    return fhi.file[file_handle].size / 512;
}

/*
 * FHI interface: read sectors from file (retry + chunking handled by udpfs_bread)
 */
int fhi_read(int file_handle, void *buffer, unsigned int sector_start, unsigned int sector_count)
{
    int32_t server_handle;
    int ret;

    M_DEBUG("%s(file_handle=%d, sector_start=%u, sector_count=%u)\n",
        __FUNCTION__, file_handle, sector_start, sector_count);

    if (file_handle < 0 || file_handle >= FHI_MAX_FILES)
        return 0;
    if (!udpfs_core_is_connected())
        return 0;

    server_handle = fhi.file[file_handle].id;
    if (server_handle < 0)
        return 0;

    WaitSema(g_sema);

    ret = udpfs_core_bread(server_handle, (uint64_t)sector_start, buffer, sector_count, 512);
    if (ret < 0) {
        M_DEBUG("udpfs_fhi: read failed\n");
        SignalSema(g_sema);
        return 0;
    }

    SignalSema(g_sema);
    return sector_count;
}

/*
 * FHI interface: write sectors to file
 */
int fhi_write(int file_handle, const void *buffer, unsigned int sector_start, unsigned int sector_count)
{
    int32_t server_handle;

    if (file_handle < 0 || file_handle >= FHI_MAX_FILES)
        return 0;
    if (!udpfs_core_is_connected())
        return 0;

    server_handle = fhi.file[file_handle].id;
    if (server_handle < 0)
        return 0;

    WaitSema(g_sema);

    if (udpfs_core_bwrite(server_handle,
            (uint64_t)sector_start, buffer, sector_count, 512) < 0) {
        M_DEBUG("udpfs_fhi: write failed\n");
        SignalSema(g_sema);
        return 0;
    }

    SignalSema(g_sema);
    return sector_count;
}


/*
 * Initialize UDPFS FHI driver (fhi_init export)
 */
int fhi_init(void)
{
    iop_sema_t sema_info;
    int ret;

    M_DEBUG("UDPFS FHI over UDPRDMA\n");

    /* Create semaphore for thread safety */
    sema_info.attr = 0;
    sema_info.initial = 1;
    sema_info.max = 1;
    g_sema = CreateSema(&sema_info);
    if (g_sema < 0) {
        M_DEBUG("udpfs_fhi: failed to create semaphore\n");
        return MODULE_NO_RESIDENT_END;
    }

    /* Initialize core UDPFS */
    ret = udpfs_core_init();
    if (ret < 0) {
        DeleteSema(g_sema);
        g_sema = -1;
        return MODULE_NO_RESIDENT_END;
    }

    M_DEBUG("udpfs_fhi: ready\n");
    return MODULE_RESIDENT_END;
}
