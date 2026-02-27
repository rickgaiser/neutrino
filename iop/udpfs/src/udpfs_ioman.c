/*
 * UDPFS iomanX - iomanX Device Wrapper for UDPFS
 *
 * Thin wrapper that translates iomanX device operations to core UDPFS protocol calls.
 * Manages file descriptor table mapping iomanX file pointers to server handles.
 */

#include <errno.h>
#include <iomanX.h>
#include <io_common.h>
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "udpfs_core.h"


#define UDPFS_MAX_HANDLES 8


/* Per-handle state for file descriptor mapping */
typedef struct {
    int32_t server_handle;  /* Server-side handle, -1 = free */
    int     is_dir;         /* 1 if directory, 0 if file */
} udpfs_fd_t;

static udpfs_fd_t g_fds[UDPFS_MAX_HANDLES];


/*
 * Helper: allocate a local file descriptor slot
 */
static int _alloc_fd(void)
{
    int i;
    for (i = 0; i < UDPFS_MAX_HANDLES; i++) {
        if (g_fds[i].server_handle < 0)
            return i;
    }
    return -1;
}

/*
 * Helper: free a local file descriptor slot
 */
static void _free_fd(int idx)
{
    if (idx >= 0 && idx < UDPFS_MAX_HANDLES)
        g_fds[idx].server_handle = -1;
}

/*
 * Helper: validate file descriptor and extract server handle.
 * Returns 0 on success, negative errno on error.
 */
static int _validate_fd(iomanX_iop_file_t *f, int *fd_idx_out, int32_t *handle_out)
{
    int fd_idx = (int)(uintptr_t)f->privdata;
    if (!udpfs_core_is_connected())
        return -EIO;
    if (fd_idx < 0 || fd_idx >= UDPFS_MAX_HANDLES)
        return -EBADF;
    if (g_fds[fd_idx].server_handle < 0)
        return -EBADF;
    *fd_idx_out = fd_idx;
    *handle_out = g_fds[fd_idx].server_handle;
    return 0;
}


/*
 * iomanX device operations
 */

static int udpfs_init_dev(iomanX_iop_device_t *d)
{
    int i, ret;

    M_DEBUG("%s()\n", __FUNCTION__);

    /* Initialize FD table */
    for (i = 0; i < UDPFS_MAX_HANDLES; i++)
        g_fds[i].server_handle = -1;

    /* Initialize core UDPFS */
    ret = udpfs_core_init();
    if (ret < 0)
        return -1;

    M_DEBUG("udpfs: ready\n");
    return 0;
}

static int udpfs_deinit_dev(iomanX_iop_device_t *d)
{
    M_DEBUG("%s()\n", __FUNCTION__);
    udpfs_core_exit();
    return 0;
}

static int udpfs_format(iomanX_iop_file_t *f, const char *unk1, const char *unk2, void *unk3, int unk4)
{
    return -EIO;
}

static int udpfs_open(iomanX_iop_file_t *f, const char *name, int flags, int mode)
{
    int fd_idx, ret;
    int32_t server_handle;

    M_DEBUG("%s(%s, 0x%x)\n", __FUNCTION__, name, flags);

    /* Allocate local fd */
    fd_idx = _alloc_fd();
    if (fd_idx < 0)
        return -EMFILE;

    /* Call core open */
    ret = udpfs_core_open(name, flags, mode, 0, &server_handle);
    if (ret < 0) {
        _free_fd(fd_idx);
        return ret;
    }

    /* Store handle and metadata */
    g_fds[fd_idx].server_handle = server_handle;
    g_fds[fd_idx].is_dir = 0;
    f->privdata = (void *)(uintptr_t)fd_idx;

    return 0;
}

static int udpfs_close(iomanX_iop_file_t *f)
{
    int fd_idx = (int)(uintptr_t)f->privdata;

    M_DEBUG("%s(fd=%d)\n", __FUNCTION__, fd_idx);

    if (fd_idx < 0 || fd_idx >= UDPFS_MAX_HANDLES)
        return -EBADF;
    if (g_fds[fd_idx].server_handle < 0)
        return -EBADF;

    /* Call core close */
    udpfs_core_close(g_fds[fd_idx].server_handle);

    _free_fd(fd_idx);
    return 0;
}

static int udpfs_read(iomanX_iop_file_t *f, void *buffer, int size)
{
    int fd_idx;
    int32_t server_handle;
    int ret;

    ret = _validate_fd(f, &fd_idx, &server_handle);
    if (ret < 0)
        return ret;

    M_DEBUG("%s(fd=%d, %d bytes)\n", __FUNCTION__, fd_idx, size);

    /* Delegate to core */
    return udpfs_core_read(server_handle, buffer, size);
}

static int udpfs_write(iomanX_iop_file_t *f, void *buffer, int size)
{
    int fd_idx;
    int32_t server_handle;
    int ret;

    ret = _validate_fd(f, &fd_idx, &server_handle);
    if (ret < 0)
        return ret;

    M_DEBUG("%s(fd=%d, %d bytes)\n", __FUNCTION__, fd_idx, size);

    /* Delegate to core */
    return udpfs_core_write(server_handle, buffer, size);
}

static s64 udpfs_lseek64(iomanX_iop_file_t *f, s64 offset, int whence)
{
    int fd_idx;
    int32_t server_handle;
    int ret;

    ret = _validate_fd(f, &fd_idx, &server_handle);
    if (ret < 0)
        return ret;

    M_DEBUG("%s(fd=%d, offset=0x%x%08x, whence=%d)\n", __FUNCTION__, fd_idx,
        (unsigned int)(offset >> 32), (unsigned int)offset, whence);

    /* Delegate to core */
    return udpfs_core_lseek(server_handle, offset, whence);
}

static int udpfs_lseek(iomanX_iop_file_t *f, int offset, int whence)
{
    M_DEBUG("%s(%d, %d)\n", __FUNCTION__, offset, whence);
    return (int)udpfs_lseek64(f, (s64)offset, whence);
}

static int udpfs_ioctl(iomanX_iop_file_t *f, int cmd, void *data)
{
    return -EIO;
}

static int udpfs_remove(iomanX_iop_file_t *f, const char *name)
{
    M_DEBUG("%s(%s)\n", __FUNCTION__, name);
    return udpfs_core_remove(name);
}

static int udpfs_mkdir(iomanX_iop_file_t *f, const char *path, int mode)
{
    M_DEBUG("%s(%s, 0x%x)\n", __FUNCTION__, path, mode);
    return udpfs_core_mkdir(path, mode);
}

static int udpfs_rmdir(iomanX_iop_file_t *f, const char *path)
{
    M_DEBUG("%s(%s)\n", __FUNCTION__, path);
    return udpfs_core_rmdir(path);
}

static int udpfs_dopen(iomanX_iop_file_t *f, const char *path)
{
    int fd_idx, ret;
    int32_t server_handle;

    M_DEBUG("%s(%s)\n", __FUNCTION__, path);

    /* Allocate local fd */
    fd_idx = _alloc_fd();
    if (fd_idx < 0)
        return -EMFILE;

    /* Call core open for directory */
    ret = udpfs_core_open(path, 0, 0, 1, &server_handle);
    if (ret < 0) {
        _free_fd(fd_idx);
        return ret;
    }

    /* Store handle and metadata */
    g_fds[fd_idx].server_handle = server_handle;
    g_fds[fd_idx].is_dir = 1;
    f->privdata = (void *)(uintptr_t)fd_idx;

    return 0;
}

static int udpfs_dclose(iomanX_iop_file_t *f)
{
    M_DEBUG("%s()\n", __FUNCTION__);
    return udpfs_close(f);
}

static int udpfs_dread(iomanX_iop_file_t *f, iox_dirent_t *dirent)
{
    int fd_idx;
    int32_t server_handle;
    iox_stat_t stat;
    char name[256];
    int ret;

    ret = _validate_fd(f, &fd_idx, &server_handle);
    if (ret < 0)
        return ret;

    M_DEBUG("%s(fd=%d)\n", __FUNCTION__, fd_idx);

    /* Call core dread */
    ret = udpfs_core_dread(server_handle, &stat, name, sizeof(name));
    if (ret <= 0)
        return ret;  /* 0 = end of dir, negative = error */

    /* Populate dirent from stat and name */
    memset(dirent, 0, sizeof(iox_dirent_t));
    memcpy(&dirent->stat, &stat, sizeof(iox_stat_t));
    strncpy(dirent->name, name, 255);
    dirent->name[255] = '\0';

    return 1;
}

static int udpfs_getstat(iomanX_iop_file_t *f, const char *name, iox_stat_t *stat)
{
    M_DEBUG("%s(%s)\n", __FUNCTION__, name);
    return udpfs_core_getstat(name, stat);
}

static int udpfs_chstat(iomanX_iop_file_t *f, const char *name, iox_stat_t *stat, unsigned int mask)
{
    return -EIO;
}

static int udpfs_rename(iomanX_iop_file_t *f, const char *old, const char *new_name)
{
    return -EIO;
}

static int udpfs_chdir(iomanX_iop_file_t *f, const char *name)
{
    return -EIO;
}

static int udpfs_sync(iomanX_iop_file_t *f, const char *dev, int flag)
{
    return -EIO;
}

static int udpfs_mount(iomanX_iop_file_t *f, const char *fsname, const char *devname, int flag, void *arg, int arglen)
{
    return -EIO;
}

static int udpfs_umount(iomanX_iop_file_t *f, const char *fsname)
{
    return -EIO;
}

static int udpfs_devctl(iomanX_iop_file_t *f, const char *name, int cmd, void *arg, unsigned int arglen, void *buf, unsigned int buflen)
{
    return -EIO;
}

static int udpfs_symlink(iomanX_iop_file_t *f, const char *old, const char *new_name)
{
    return -EIO;
}

static int udpfs_readlink(iomanX_iop_file_t *f, const char *path, char *buf, unsigned int buflen)
{
    return -EIO;
}

static int udpfs_ioctl2(iomanX_iop_file_t *f, int cmd, void *data, unsigned int datalen, void *rdata, unsigned int rdatalen)
{
    if (cmd == 0x80) {
        int fd_idx = (int)(uintptr_t)f->privdata;
        if (fd_idx >= 0 && fd_idx < UDPFS_MAX_HANDLES)
            return g_fds[fd_idx].server_handle;
        return -EBADF;
    }
    return -EIO;
}


/*
 * Device ops table
 */
static iomanX_iop_device_ops_t udpfs_device_ops = {
    udpfs_init_dev,
    udpfs_deinit_dev,
    udpfs_format,
    udpfs_open,
    udpfs_close,
    udpfs_read,
    udpfs_write,
    udpfs_lseek,
    udpfs_ioctl,
    udpfs_remove,
    udpfs_mkdir,
    udpfs_rmdir,
    udpfs_dopen,
    udpfs_dclose,
    udpfs_dread,
    udpfs_getstat,
    udpfs_chstat,
    /* Extended ops */
    udpfs_rename,
    udpfs_chdir,
    udpfs_sync,
    udpfs_mount,
    udpfs_umount,
    udpfs_lseek64,
    udpfs_devctl,
    udpfs_symlink,
    udpfs_readlink,
    udpfs_ioctl2
};

static const char udpfs_name[] = "udpfs";
static iomanX_iop_device_t udpfs_device = {
    udpfs_name,
    IOP_DT_FSEXT | IOP_DT_FS,
    1,
    udpfs_name,
    &udpfs_device_ops
};


/*
 * Initialize UDPFS as iomanX device
 */
int udpfs_init(void)
{
    M_DEBUG("UDPFS over UDPRDMA by Maximus32\n");

    /* Register iomanX device */
    AddDrv(&udpfs_device);

    return 0;
}
