/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include "internal.h"
#include "cdvdman_read.h"

#define CDVDMAN_FS_BUF_ALIGNMENT 64
static u8 *cdvdman_fs_buf;
static int cdrom_sema;
#define CDVDMAN_MODULE_VERSION 0x225
static int cdvdman_debug_print_flag = 0;

typedef struct
{
    iop_file_t *f;
    u32 lsn;
    unsigned int filesize;
    unsigned int position;
} FHANDLE;

#define MAX_FDHANDLES 64
FHANDLE cdvdman_fdhandles[MAX_FDHANDLES];

// for "cdrom" ioctl2
#define CIOCSTREAMPAUSE  0x630D
#define CIOCSTREAMRESUME 0x630E
#define CIOCSTREAMSTAT   0x630F

// driver ops protypes
static int cdrom_dummy(void);
static s64 cdrom_dummy64(void);
static int cdrom_init(iop_device_t *dev);
static int cdrom_deinit(iop_device_t *dev);
static int cdrom_open(iop_file_t *f, const char *filename, int mode);
static int cdrom_close(iop_file_t *f);
static int cdrom_read(iop_file_t *f, void *buf, int size);
static int cdrom_lseek(iop_file_t *f, int offset, int where);
static int cdrom_getstat(iop_file_t *f, const char *filename, iox_stat_t *stat);
static int cdrom_dopen(iop_file_t *f, const char *dirname);
static int cdrom_dread(iop_file_t *f, iox_dirent_t *dirent);
static int cdrom_ioctl(iop_file_t *f, u32 cmd, void *args);
static int cdrom_devctl(iop_file_t *f, const char *name, int cmd, void *args, u32 arglen, void *buf, u32 buflen);
static int cdrom_ioctl2(iop_file_t *f, int cmd, void *args, unsigned int arglen, void *buf, unsigned int buflen);

// driver ops func tab
static iop_ext_device_ops_t cdrom_ops = {
    &cdrom_init,
    &cdrom_deinit,
    (void *)&cdrom_dummy,
    &cdrom_open,
    &cdrom_close,
    &cdrom_read,
    (void *)&cdrom_dummy,
    &cdrom_lseek,
    &cdrom_ioctl,
    (void *)&cdrom_dummy,
    (void *)&cdrom_dummy,
    (void *)&cdrom_dummy,
    &cdrom_dopen,
    &cdrom_close, // dclose -> close
    &cdrom_dread,
    &cdrom_getstat,
    (void *)&cdrom_dummy,
    (void *)&cdrom_dummy,
    (void *)&cdrom_dummy,
    (void *)&cdrom_dummy,
    (void *)&cdrom_dummy,
    (void *)&cdrom_dummy,
    (void *)&cdrom_dummy64,
    (void *)&cdrom_devctl,
    (void *)&cdrom_dummy,
    (void *)&cdrom_dummy,
    &cdrom_ioctl2};

// driver descriptor
static iop_ext_device_t cdrom_dev = {
    "cdrom",
    IOP_DT_FS | IOP_DT_FSEXT,
    1,
    "CD-ROM ",
    &cdrom_ops};

//--------------------------------------------------------------
static FHANDLE *cdvdman_getfilefreeslot(void)
{
    int i;
    FHANDLE *fh;

    for (i = 0; i < MAX_FDHANDLES; i++) {
        fh = (FHANDLE *)&cdvdman_fdhandles[i];
        if (fh->f == NULL)
            return fh;
    }

    return 0;
}

//--------------------------------------------------------------
static int cdvdman_open(iop_file_t *f, const char *filename, int mode)
{
    int r = 0;
    FHANDLE *fh;
    sceCdlFILE cdfile;

    cdvdman_init();

    if (f->unit < 2) {
        sceCdDiskReady(0);

        fh = cdvdman_getfilefreeslot();
        if (fh) {
            r = sceCdLayerSearchFile(&cdfile, filename, f->unit);
            if (r) {
                f->privdata = fh;
                fh->f = f;
                fh->filesize = cdfile.size;
                fh->lsn = cdfile.lsn;
                fh->position = 0;
                r = 0;

                M_DEBUG("open ret=%d lsn=%d size=%d\n", r, (int)fh->lsn, (int)fh->filesize);
            } else
                r = -ENOENT;
        } else
            r = -EMFILE;
    } else
        r = -ENOENT;

    return r;
}

//--------------------------------------------------------------
// SCE does this too, hence assuming that the version suffix will be either totally there or absent. The only version supported is 1.
// Instead of using strcat like the original, append the version suffix manually for efficiency.
static int cdrom_purifyPath(char *path)
{
    int len;

    len = strlen(path);

    // Adjusted to better handle cases. Was adding ;1 on every case no matter what.

    if (len >= 3) {
        // Path is already valid.
        if ((path[len - 2] == ';') && (path[len - 1] == '1')) {
            return 1;
        }

        // Path is missing only version.
        if (path[len - 1] == ';') {
            path[len] = '1';
            path[len + 1] = '\0';
            return 0;
        }

        // Path has no terminator or version at all.
        path[len] = ';';
        path[len + 1] = '1';
        path[len + 2] = '\0';

        return 0;
    }

    return 1;
}

//--------------------------------------------------------------
static int cdrom_dummy(void)
{
    M_DEBUG("%s\n", __FUNCTION__);
    return -EPERM;
}

//--------------------------------------------------------------
static s64 cdrom_dummy64(void)
{
    M_DEBUG("%s\n", __FUNCTION__);
    return -EPERM;
}

//--------------------------------------------------------------
static int cdrom_init(iop_device_t *dev)
{
    M_DEBUG("%s\n", __FUNCTION__);
    return 0;
}

//--------------------------------------------------------------
static int cdrom_deinit(iop_device_t *dev)
{
    M_DEBUG("%s\n", __FUNCTION__);
    return 0;
}

//--------------------------------------------------------------
static int cdrom_open(iop_file_t *f, const char *filename, int mode)
{
    int result;
    char path_buffer[128]; // Original buffer size in the SCE CDVDMAN module.

    WaitSema(cdrom_sema);

    M_DEBUG("%s %s mode=%d layer %d\n", __FUNCTION__, filename, mode, f->unit);

    strncpy(path_buffer, filename, sizeof(path_buffer));
    cdrom_purifyPath(path_buffer);

    if ((result = cdvdman_open(f, path_buffer, mode)) >= 0)
        f->mode = O_RDONLY; // SCE fixes the open flags to O_RDONLY for open().

    SignalSema(cdrom_sema);

    return result;
}

//--------------------------------------------------------------
static int cdrom_close(iop_file_t *f)
{
    FHANDLE *fh = (FHANDLE *)f->privdata;

    WaitSema(cdrom_sema);

    M_DEBUG("%s\n", __FUNCTION__);

    memset(fh, 0, sizeof(FHANDLE));
    f->mode = 0; // SCE invalidates FDs by clearing the open flags.

    SignalSema(cdrom_sema);

    return 0;
}

//--------------------------------------------------------------
static int cdrom_read(iop_file_t *f, void *buf, int size)
{
    FHANDLE *fh = (FHANDLE *)f->privdata;
    unsigned int offset, nsectors, nbytes;
    int rpos;

    WaitSema(cdrom_sema);

    M_DEBUG("%s(..., 0x%x, %ds + %db) file_position=%d\n", __FUNCTION__, buf, size / 2048, size % 2048, fh->position);

    if ((fh->position + size) > fh->filesize)
        size = fh->filesize - fh->position;

    sceCdDiskReady(0);

    rpos = 0;
    if (size > 0) {
        // Phase 1: read data until the offset of the file is nicely aligned to a 2048-byte boundary.
        if ((offset = fh->position % 2048) != 0) {
            nbytes = 2048 - offset;
            if (size < nbytes)
                nbytes = size;
            while (sceCdRead_internal(fh->lsn + (fh->position / 2048), 1, cdvdman_fs_buf, NULL, ECS_IOOPS) == 0)
                DelayThread(10000);

            fh->position += nbytes;
            size -= nbytes;
            rpos += nbytes;

            sceCdSync(0);
            memcpy(buf, &cdvdman_fs_buf[offset], nbytes);
            buf = (void *)((u8 *)buf + nbytes);
        }

        // Phase 2: read the data to the middle of the buffer, in units of 2048.
        if ((nsectors = size / 2048) > 0) {
            nbytes = nsectors * 2048;

            while (sceCdRead_internal(fh->lsn + (fh->position / 2048), nsectors, buf, NULL, ECS_IOOPS) == 0)
                DelayThread(10000);

            buf += nbytes;
            size -= nbytes;
            fh->position += nbytes;
            rpos += nbytes;

            sceCdSync(0);
        }

        // Phase 3: read any remaining data that isn't divisible by 2048.
        if ((nbytes = size) > 0) {
            while (sceCdRead_internal(fh->lsn + (fh->position / 2048), 1, cdvdman_fs_buf, NULL, ECS_IOOPS) == 0)
                DelayThread(10000);

            fh->position += nbytes;
            rpos += nbytes;

            sceCdSync(0);
            memcpy(buf, cdvdman_fs_buf, nbytes);
        }
    }

    //M_DEBUG("cdrom_read ret=%d\n", rpos);
    SignalSema(cdrom_sema);

    return rpos;
}

//--------------------------------------------------------------
static int cdrom_lseek(iop_file_t *f, int offset, int where)
{
    int r;
    FHANDLE *fh = (FHANDLE *)f->privdata;

    WaitSema(cdrom_sema);

    M_DEBUG("%s offset=%d where=%d\n", __FUNCTION__, offset, where);

    switch (where) {
        case SEEK_CUR:
            r = fh->position += offset;
            break;
        case SEEK_SET:
            r = fh->position = offset;
            break;
        case SEEK_END:
            r = fh->position = fh->filesize - offset;
            break;
        default:
            r = fh->position;
    }

    if (fh->position > fh->filesize)
        r = fh->position = fh->filesize;

    M_DEBUG("%s file offset=%d\n", __FUNCTION__, fh->position);
    SignalSema(cdrom_sema);

    return r;
}

//--------------------------------------------------------------
static int cdrom_getstat(iop_file_t *f, const char *filename, iox_stat_t *stat)
{
    int r;
    char path_buffer[128]; // Original buffer size in the SCE CDVDMAN module.

    WaitSema(cdrom_sema);

    M_DEBUG("%s %s layer %d\n", __FUNCTION__, filename, f->unit);

    strncpy(path_buffer, filename, sizeof(path_buffer));
    cdrom_purifyPath(path_buffer); // Unlike the SCE original, purify the path right away.

    sceCdDiskReady(0);
    r = sceCdLayerSearchFile((sceCdlFILE *)&stat->attr, path_buffer, f->unit) - 1;

    SignalSema(cdrom_sema);

    return r;
}

//--------------------------------------------------------------
static int cdrom_dopen(iop_file_t *f, const char *dirname)
{
    int r;

    WaitSema(cdrom_sema);

    M_DEBUG("%d %s layer %d\n", __FUNCTION__, dirname, f->unit);

    r = cdvdman_open(f, dirname, 8);

    SignalSema(cdrom_sema);

    return r;
}

//--------------------------------------------------------------
static int cdrom_dread(iop_file_t *f, iox_dirent_t *dirent)
{
    int r = 0;
    u32 mode;
    FHANDLE *fh = (FHANDLE *)f->privdata;
    struct dirTocEntry *tocEntryPointer;

    WaitSema(cdrom_sema);

    M_DEBUG("%s fh->lsn=%lu\n", __FUNCTION__, fh->lsn);

    sceCdDiskReady(0);
    if ((r = sceCdRead_internal(fh->lsn, 1, cdvdman_fs_buf, NULL, ECS_IOOPS)) == 1) {
        sceCdSync(0);

        do {
            r = 0;
            tocEntryPointer = (struct dirTocEntry *)&cdvdman_fs_buf[fh->position];
            if (tocEntryPointer->length == 0)
                break;

            fh->position += tocEntryPointer->length;
            r = 1;
        } while (tocEntryPointer->filenameLength == 1);

        mode = 0x2124;
        if (tocEntryPointer->fileProperties & 2)
            mode = 0x116d;

        dirent->stat.mode = mode;
        dirent->stat.size = tocEntryPointer->fileSize;
        strncpy(dirent->name, tocEntryPointer->filename, tocEntryPointer->filenameLength);
        dirent->name[tocEntryPointer->filenameLength] = '\0';

        M_DEBUG("%s r=%d mode=%04x name=%s\n", __FUNCTION__, r, (int)mode, dirent->name);
    } else
        M_DEBUG("%s r=%d\n", __FUNCTION__, r);

    SignalSema(cdrom_sema);

    return r;
}

//--------------------------------------------------------------
static int cdrom_ioctl(iop_file_t *f, u32 cmd, void *args)
{
    int r = 0;

    M_DEBUG("%s 0x%X\n", __FUNCTION__, cmd);

    WaitSema(cdrom_sema);

    if (cmd != 0x10000) // Spin Ctrl op
        r = -EINVAL;

    SignalSema(cdrom_sema);

    return r;
}

//--------------------------------------------------------------
static int cdrom_devctl(iop_file_t *f, const char *name, int cmd, void *args, u32 arglen, void *buf, u32 buflen)
{
    int result;

    M_DEBUG("%s cmd=0x%X\n", __FUNCTION__, cmd);

    WaitSema(cdrom_sema);

    result = 0;
    switch (cmd) {
        case CDIOC_READCLOCK:
            result = sceCdReadClock((sceCdCLOCK *)buf);
            if (result != 1)
                result = -EIO;
            break;
        case CDIOC_READGUID:
            result = sceCdReadGUID(buf);
            break;
        case CDIOC_READDISKGUID:
            result = sceCdReadDiskID(buf);
            break;
        case CDIOC_GETDISKTYPE:
            *(int *)buf = sceCdGetDiskType();
            break;
        case CDIOC_GETERROR:
            *(int *)buf = sceCdGetError();
            break;
        case CDIOC_TRAYREQ:
            result = sceCdTrayReq(*(int *)args, (u32 *)buf);
            if (result != 1)
                result = -EIO;
            break;
        case CDIOC_STATUS:
            *(int *)buf = sceCdStatus();
            break;
        case CDIOC_POWEROFF:
            result = sceCdPowerOff((u32 *)args);
            if (result != 1)
                result = -EIO;
            break;
        case CDIOC_MMODE:
            result = 1;
            break;
        case CDIOC_DISKRDY:
            *(int *)buf = sceCdDiskReady(*(int *)args);
            break;
        case CDIOC_READMODELID:
            result = sceCdReadModelID(buf);
            break;
        case CDIOC_STREAMINIT:
            result = sceCdStInit(((u32 *)args)[0], ((u32 *)args)[1], (void *)((u32 *)args)[2]);
            break;
        case CDIOC_BREAK:
            result = sceCdBreak_internal(ECS_IOOPS);
            if (result != 1)
                result = -EIO;
            sceCdSync(0);
            break;
        case CDIOC_SPINNOM:
        case CDIOC_SPINSTM:
        case CDIOC_TRYCNT:
        case CDIOC_READDVDDUALINFO:
        case CDIOC_INIT:
            result = 0;
            break;
        case CDIOC_STANDBY:
            result = sceCdStandby_internal(ECS_IOOPS);
            if (result != 1)
                result = -EIO;
            sceCdSync(0);
            break;
        case CDIOC_STOP:
            result = sceCdStop_internal(ECS_IOOPS);
            if (result != 1)
                result = -EIO;
            sceCdSync(0);
            break;
        case CDIOC_PAUSE:
            result = sceCdPause_internal(ECS_IOOPS);
            if (result != 1)
                result = -EIO;
            sceCdSync(0);
            break;
        case CDIOC_GETTOC:
            result = sceCdGetToc_internal(buf, ECS_IOOPS);
            if (result != 1)
                result = -EIO;
            sceCdSync(0);
            break;
        case CDIOC_GETINTREVENTFLG:
            *(int *)buf = cdvdman_stat.intr_ef;
            result = cdvdman_stat.intr_ef;
            break;
        default:
            M_DEBUG("%s unknown, cmd=0x%X\n", __FUNCTION__, cmd);
            result = -EIO;
            break;
    }

    SignalSema(cdrom_sema);

    return result;
}

//--------------------------------------------------------------
static int cdrom_ioctl2(iop_file_t *f, int cmd, void *args, unsigned int arglen, void *buf, unsigned int buflen)
{
    int r = 0;

    // There was a check here on whether the file was opened with mode 8.

    M_DEBUG("%s\n", __FUNCTION__);

    WaitSema(cdrom_sema);

    switch (cmd) {
        case CIOCSTREAMPAUSE:
            r = sceCdStPause();
            break;
        case CIOCSTREAMRESUME:
            r = sceCdStResume();
            break;
        case CIOCSTREAMSTAT:
            r = sceCdStStat();
            break;
        default:
            r = -EINVAL;
    }

    SignalSema(cdrom_sema);

    return r;
}

//-------------------------------------------------------------------------
void *sceGetFsvRbuf2(int *size)
{
    M_DEBUG("%s\n", __FUNCTION__);

    *size = cdvdman_settings.fs_sectors * 2048 + CDVDMAN_FS_BUF_ALIGNMENT;
    return cdvdman_fs_buf;
}

//-------------------------------------------------------------------------
int sceCdSC(int code, int *param)
{
    int result;

    M_DEBUG("%s(0x%X, 0x%X)\n", __FUNCTION__, code, *param);

    switch (code) {
        case CDSC_GET_INTRFLAG:
            result = cdvdman_stat.intr_ef;
            break;
        case CDSC_IO_SEMA:
            if (*param) {
                WaitSema(cdrom_sema);
            } else
                SignalSema(cdrom_sema);

            result = *param; // EE N-command code.
            break;
        case CDSC_GET_VERSION:
            result = CDVDMAN_MODULE_VERSION;
            break;
        case CDSC_GET_DEBUG_STATUS:
            *param = (int)&cdvdman_debug_print_flag;
            result = 0xFF;
            break;
        case CDSC_SET_ERROR:
            result = cdvdman_stat.err = *param;
            break;
        default:
            M_DEBUG("%s(0x%X, 0x%X) unknown code\n", __FUNCTION__, code, *param);
            result = 1; // dummy result
    }

    return result;
}

//-------------------------------------------------------------------------
void cdvdman_initdev(void)
{
    iop_sema_t smp;

    M_DEBUG("%s\n", __FUNCTION__);

    // Create semaphore
    smp.initial = 1;
    smp.max = 1;
    smp.attr = 1;
    smp.option = 0;
    cdrom_sema = CreateSema(&smp);

    // Limit min/max sectors
    if (cdvdman_settings.fs_sectors < 2)
        cdvdman_settings.fs_sectors = 2;
    if (cdvdman_settings.fs_sectors > 128)
        cdvdman_settings.fs_sectors = 128;
    // Allocate sector buffer
    cdvdman_fs_buf = AllocSysMemory(0, cdvdman_settings.fs_sectors * 2048 + CDVDMAN_FS_BUF_ALIGNMENT, NULL);

    memset(&cdvdman_fdhandles[0], 0, MAX_FDHANDLES * sizeof(FHANDLE));

    DelDrv("cdrom");
    AddDrv((iop_device_t *)&cdrom_dev);
}
