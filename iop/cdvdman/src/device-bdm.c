/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include "internal.h"

#include <bdm.h>
#include <bd_defrag.h>

#include "device.h"

extern struct cdvdman_settings_bdm cdvdman_settings;
static struct block_device *g_bd = NULL;
static u32 g_bd_sectors_per_sector = 4;
static int bdm_io_sema;

extern struct irx_export_table _exp_bdm;

//
// BDM exported functions
//

void bdm_connect_bd(struct block_device *bd)
{
    DPRINTF("connecting device %s%dp%d\n", bd->name, bd->devNr, bd->parNr);

    if (g_bd == NULL) {
        g_bd = bd;
        g_bd_sectors_per_sector = (2048 / bd->sectorSize);
        // Free usage of block device
        SignalSema(bdm_io_sema);
    }
}

void bdm_disconnect_bd(struct block_device *bd)
{
    DPRINTF("disconnecting device %s%dp%d\n", bd->name, bd->devNr, bd->parNr);

    // Lock usage of block device
    WaitSema(bdm_io_sema);
    if (g_bd == bd)
        g_bd = NULL;
}

//
// cdvdman "Device" functions
//

void DeviceInit(void)
{
    iop_sema_t smp;

    DPRINTF("%s\n", __func__);

    // Create semaphore, initially locked
    smp.initial = 0;
    smp.max = 1;
    smp.option = 0;
    smp.attr = SA_THPRI;
    bdm_io_sema = CreateSema(&smp);

    RegisterLibraryEntries(&_exp_bdm);
}

void DeviceDeinit(void)
{
    DPRINTF("%s\n", __func__);
}

int DeviceReady(void)
{
    // DPRINTF("%s\n", __func__);

    return (g_bd == NULL) ? SCECdNotReady : SCECdComplete;
}

void DeviceStop(void)
{
    DPRINTF("%s\n", __func__);

    if (g_bd != NULL)
        g_bd->stop(g_bd);
}

void DeviceFSInit(void)
{
    DPRINTF("Waiting for device...\n");
    WaitSema(bdm_io_sema);
    DPRINTF("Waiting for device...done!\n");
    SignalSema(bdm_io_sema);
}

void DeviceLock(void)
{
    DPRINTF("%s\n", __func__);

    WaitSema(bdm_io_sema);
}

void DeviceUnmount(void)
{
    DPRINTF("%s\n", __func__);
}

int DeviceReadSectors(u32 vlsn, void *buffer, unsigned int sectors)
{
    int rv = SCECdErNO;
    u32 fid = vlsn >> 23;
    u32 lsn = vlsn & ((1<<23)-1);

    // DPRINTF("%s(%u-%u, 0x%p, %u)\n", __func__, (unsigned int)fid, (unsigned int)lsn, buffer, sectors);

    if (g_bd == NULL)
        return SCECdErTRMOPN;

    WaitSema(bdm_io_sema);
    if (bd_defrag(g_bd, cdvdman_settings.fragfile[fid].frag_count, &cdvdman_settings.frags[cdvdman_settings.fragfile[fid].frag_start], lsn * 4, buffer, sectors * 4) != (sectors * 4))
        rv = SCECdErREAD;
    SignalSema(bdm_io_sema);

    return rv;
}
