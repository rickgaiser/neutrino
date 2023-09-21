#include <loadcore.h>
#include <stdio.h>
#include <sysclib.h>
#include <thsemap.h>
#include <thbase.h>
#include <bdm.h>
#include <bd_defrag.h>

#include "fhi.h"
#include "fhi_bdm.h"
#include "mprintf.h"

#define MODNAME "fhi" // give all fhi modules the same name
IRX_ID(MODNAME, 1, 1);

struct fhi_bdm fhi = {MODULE_SETTINGS_MAGIC};
static struct block_device *g_bd = NULL;
static int bdm_io_sema;

extern struct irx_export_table _exp_bdm;
extern struct irx_export_table _exp_fhi;

//---------------------------------------------------------------------------
// BDM export #4
void bdm_connect_bd(struct block_device *bd)
{
    M_DEBUG("connecting device %s%dp%d\n", bd->name, bd->devNr, bd->parNr);

    if (strncmp(bd->name, (char *)&fhi.drvName, 4)) {
        M_DEBUG("- skipping wrong driver\n");
        return;
    }

    if (bd->devNr != fhi.devNr) {
        M_DEBUG("- skipping wrong device nr\n");
        return;
    }

    if (g_bd != NULL) {
        M_DEBUG("- ERROR: device already connected\n");
        return;
    }

    g_bd = bd;
    // Free usage of block device
    SignalSema(bdm_io_sema);
}

//---------------------------------------------------------------------------
// BDM export #5
void bdm_disconnect_bd(struct block_device *bd)
{
    M_DEBUG("disconnecting device %s%dp%d\n", bd->name, bd->devNr, bd->parNr);

    // Lock usage of block device
    WaitSema(bdm_io_sema);
    if (g_bd == bd)
        g_bd = NULL;
}

//---------------------------------------------------------------------------
// FHI export #4
u32 fhi_size(int file_handle)
{
    if (file_handle < 0 || file_handle >= BDM_MAX_FILES)
        return 0;

    M_DEBUG("%s(%d)\n", __func__, file_handle);

    return fhi.fragfile[file_handle].size / 512;
}

//---------------------------------------------------------------------------
// FHI export #5
int fhi_read(int file_handle, void *buffer, unsigned int sector_start, unsigned int sector_count)
{
    int rv;
    struct fhi_bdm_fragfile *ff;

    if (file_handle)
        M_DEBUG("%s(%d, 0x%x, %d, %d)\n", __func__, file_handle, buffer, sector_start, sector_count);

    if (file_handle < 0 || file_handle >= BDM_MAX_FILES)
        return -1;

    ff = &fhi.fragfile[file_handle];
    WaitSema(bdm_io_sema);
    rv = bd_defrag_read(g_bd, ff->frag_count, &fhi.frags[ff->frag_start],sector_start, buffer, sector_count);
    SignalSema(bdm_io_sema);

    return rv;
}

//---------------------------------------------------------------------------
// FHI export #6
int fhi_write(int file_handle, const void *buffer, unsigned int sector_start, unsigned int sector_count)
{
    int rv;
    struct fhi_bdm_fragfile *ff;

    if (file_handle)
        M_DEBUG("%s(%d, 0x%x, %d, %d)\n", __func__, file_handle, buffer, sector_start, sector_count);

    if (file_handle < 0 || file_handle >= BDM_MAX_FILES)
        return -1;

    ff = &fhi.fragfile[file_handle];
    WaitSema(bdm_io_sema);
    rv = bd_defrag_write(g_bd, ff->frag_count, &fhi.frags[ff->frag_start],sector_start, buffer, sector_count);
    SignalSema(bdm_io_sema);

    return rv;
}

//---------------------------------------------------------------------------
#ifdef DEBUG
static void watchdog_thread()
{
    while (1) {
        M_DEBUG("FHI alive\n");
        DelayThread(5 * 1000 * 1000); // 5s
    }
}
#endif

//---------------------------------------------------------------------------
int _start(int argc, char **argv)
{
    iop_sema_t smp;
#ifdef DEBUG
    int th;
    iop_thread_t ThreadData;
#endif

    M_DEBUG("%s\n", __func__);

#ifdef DEBUG
    ThreadData.attr = TH_C;
    ThreadData.thread = (void *)watchdog_thread;
    ThreadData.option = 0;
    ThreadData.priority = 40;
    ThreadData.stacksize = 0x1000;
    th = CreateThread(&ThreadData);
    StartThread(th, 0);
#endif

    // Create semaphore, initially locked
    smp.initial = 0;
    smp.max = 1;
    smp.option = 0;
    smp.attr = SA_THPRI;
    bdm_io_sema = CreateSema(&smp);

    RegisterLibraryEntries(&_exp_bdm);
    RegisterLibraryEntries(&_exp_fhi);

    return MODULE_RESIDENT_END;
}
