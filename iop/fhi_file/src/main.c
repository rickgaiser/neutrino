#include <ioman.h>
#include <loadcore.h>
#include <stdio.h>
#include <sysclib.h>
#include <thsemap.h>
#include <thbase.h>

#include "fhi_file.h"
#include "mprintf.h"

#define MODNAME "fhi" // give all fhi modules the same name
IRX_ID(MODNAME, 1, 1);

struct fhi_file fhi = {MODULE_SETTINGS_MAGIC};

extern struct irx_export_table _exp_fhi;

//---------------------------------------------------------------------------
// FHI export #4
u32 fhi_size(int file_handle)
{
    if (file_handle < 0 || file_handle >= FHI_MAX_FILES)
        return 0;

    M_DEBUG("%s(%d)\n", __func__, file_handle);

    return fhi.file[file_handle].size / 512;
}

//---------------------------------------------------------------------------
// FHI export #5
int fhi_read(int file_handle, void *buffer, unsigned int sector_start, unsigned int sector_count)
{
    int rv = 0;
    struct fhi_file_info *ff;

    if (file_handle)
        M_DEBUG("%s(%d, 0x%x, %d, %d)\n", __func__, file_handle, buffer, sector_start, sector_count);

    if (file_handle < 0 || file_handle >= FHI_MAX_FILES)
        return -1;

    ff = &fhi.file[file_handle];

    // Make nicer!
    int fd = open(ff->name, FIO_O_RDONLY);
    if (fd > 0) {
        lseek(fd, sector_start * 512, FIO_SEEK_SET);
        if (read(fd, buffer, sector_count * 512) == sector_count * 512)
            rv = sector_count;
        close(fd);
    }

    return rv;
}

//---------------------------------------------------------------------------
// FHI export #6
int fhi_write(int file_handle, const void *buffer, unsigned int sector_start, unsigned int sector_count)
{
    int rv;
    //struct fhi_file_info *ff;

    if (file_handle)
        M_DEBUG("%s(%d, 0x%x, %d, %d)\n", __func__, file_handle, buffer, sector_start, sector_count);

    if (file_handle < 0 || file_handle >= FHI_MAX_FILES)
        return -1;

    //ff = &fhi.file[file_handle];

    // TODO!

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

    RegisterLibraryEntries(&_exp_fhi);

    return MODULE_RESIDENT_END;
}
