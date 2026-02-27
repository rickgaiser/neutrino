/*
 * UDPFS BD - Block Device over UDPFS/UDPRDMA
 *
 * Provides block device access over network using UDPRDMA for reliable transport.
 * Uses UDPFS block I/O subset (BREAD/BWRITE) with handle 0.
 * Block device capacity is queried via GETSTAT; sectors are fixed at 512 bytes.
 */

#include <errno.h>
#include <bdm.h>
#include <thbase.h>
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "udpfs_core.h"


/* Block device handle - pre-opened by server */
#define BLOCK_DEVICE_HANDLE 0

/* State */
static struct block_device g_udpbd;


/*
 * Block device read (retry + chunking handled by udpfs_bread)
 */
static int udpfs_bd_read(struct block_device *bd, uint64_t sector, void *buffer, uint16_t count)
{
    int ret;

    if (!udpfs_core_is_connected())
        return -EIO;

    if (sector >= bd->sectorCount)
        return -EINVAL;

    if ((sector + count) > bd->sectorCount)
        count = bd->sectorCount - sector;

    ret = udpfs_core_bread(BLOCK_DEVICE_HANDLE, sector, buffer, count, bd->sectorSize);
    if (ret < 0) {
        M_DEBUG("udpfs_bd: read failed\n");
        bdm_disconnect_bd(&g_udpbd);
        return ret;
    }

    return count;
}


/*
 * Write sectors via shared block I/O helper (zero-copy)
 */
static int udpfs_bd_write(struct block_device *bd, uint64_t sector, const void *buffer, uint16_t count)
{
    M_DEBUG("udpfs_bd: write sector=%u, count=%u\n", (uint32_t)sector, count);

    if (!udpfs_core_is_connected())
        return -EIO;

    if (sector >= bd->sectorCount)
        return -EINVAL;

    if ((sector + count) > bd->sectorCount)
        count = bd->sectorCount - sector;

    if (udpfs_core_bwrite(BLOCK_DEVICE_HANDLE, sector,
            buffer, count, bd->sectorSize) < 0)
        return -EIO;

    return count;
}


static void udpfs_bd_flush(struct block_device *bd)
{
    M_DEBUG("udpfs_bd: flush\n");
}


static int udpfs_bd_stop(struct block_device *bd)
{
    M_DEBUG("udpfs_bd: stop\n");
    return 0;
}


/*
 * Initialize UDPFS block device driver
 */
int udpfs_bd_init(void)
{
    int ret;

    M_DEBUG("UDPFS BD over UDPRDMA\n");

    /* Initialize core UDPFS */
    ret = udpfs_core_init();
    if (ret < 0) {
        return -1;
    }

    /* Get disk capacity via GETSTAT (returns sector count) */
    ret = udpfs_core_get_sector_count(BLOCK_DEVICE_HANDLE);
    if (ret < 0) {
        udpfs_core_exit();
        return -1;
    }

    /* Setup block device with fixed 512-byte sectors */
    g_udpbd.name = "udp";
    g_udpbd.path = "udpbd";
    g_udpbd.devNr = 0;
    g_udpbd.parNr = 0;
    g_udpbd.sectorOffset = 0;
    g_udpbd.sectorSize = 512;      /* Fixed sector size */
    g_udpbd.sectorCount = ret;     /* From GETSTAT / 512 */
    g_udpbd.priv = NULL;
    g_udpbd.read = udpfs_bd_read;
    g_udpbd.write = udpfs_bd_write;
    g_udpbd.flush = udpfs_bd_flush;
    g_udpbd.stop = udpfs_bd_stop;

    /* Connect to BDM */
    bdm_connect_bd(&g_udpbd);

    M_DEBUG("udpfs_bd: ready (sectorSize=%u, sectorCount=%u)\n",
            g_udpbd.sectorSize, g_udpbd.sectorCount);
    return 0;
}
