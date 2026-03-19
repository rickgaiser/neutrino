// libc/newlib
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
_off64_t lseek64(int __filedes, _off64_t __offset, int __whence); // should be in unistd.h

// PS2SDK
#include <kernel.h>      // nopdelay
#include <ps2sdkapi.h>   // ps2sdk_get_iop_fd

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <io_common.h>

// IOP settings structs
#include "../../../common/include/fhi_bd_config.h"    // fhi_bd, fhi_bd_file — also pulls usbhdfsd-common.h
#include "../../../common/include/fhi_fileid_config.h"

#include "fhi_config.h"
#include "modlist.h"

// ---------------------------------------------------------------------------
// Settings pointers — exactly one is non-NULL after fhi_config_init
// ---------------------------------------------------------------------------
static struct fhi_bd     *g_bd   = NULL;
static struct fhi_fileid *g_fileid = NULL;

// ---------------------------------------------------------------------------
// Vtable
// ---------------------------------------------------------------------------
struct fhi_backend_ops {
    int (*add_file_fd)(int fhi_fid, int fd, const char *path);
    int keep_open;  // 1 = leave fd open for IOP post-reboot
};

// Forward declarations of backend implementations
static int backend_bd_add_file_fd(int fhi_fid, int fd, const char *path);
static int backend_fileid_add_file_fd(int fhi_fid, int fd, const char *path);

static const struct fhi_backend_ops ops_bd   = { backend_bd_add_file_fd,   0 };
static const struct fhi_backend_ops ops_fileid = { backend_fileid_add_file_fd, 1 };

static const struct fhi_backend_ops *g_ops = NULL;

// ---------------------------------------------------------------------------
// fhi_config_init — filename-based backend detection
// ---------------------------------------------------------------------------
int fhi_config_init(struct SModList *ml)
{
    static const struct {
        const char                     *filename;
        const struct fhi_backend_ops   *ops;
        unsigned int                    settings_size;
        void                          **settings_out;
    } candidates[] = {
        { "fhi_bd.irx",    &ops_bd,     sizeof(struct fhi_bd),             (void **)&g_bd   },
        { "mmcefhi.irx",   &ops_fileid, sizeof(struct fhi_fileid),         (void **)&g_fileid },
        { "udpfs_fhi.irx", &ops_fileid, sizeof(struct fhi_fileid),         (void **)&g_fileid },
    };
    unsigned int i;

    for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        void *s = module_get_settings(modlist_get_by_name(ml, candidates[i].filename));
        if (!s)
            continue;
        memset(s, 0, candidates[i].settings_size);
        *candidates[i].settings_out = s;
        g_ops = candidates[i].ops;
        return 0;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
int fhi_add_file_fd(int fhi_fid, int fd, const char *path)
{
    int rv = g_ops->add_file_fd(fhi_fid, fd, path);
    if (!g_ops->keep_open)
        close(fd);
    return rv;
}

int fhi_add_file(int fhi_fid, const char *path, int flags)
{
    int i, fd, rv;

    printf("Loading %s...\n", path);
    for (i = 0; i < 1000; i++) {
        fd = open(path, flags);
        if (fd >= 0)
            break;
        nopdelay();
    }
    if (fd < 0) {
        printf("Unable to open %s\n", path);
        return -1;
    }

    rv = g_ops->add_file_fd(fhi_fid, fd, path);
    if (!g_ops->keep_open)
        close(fd);
    return rv;
}

// ---------------------------------------------------------------------------
// Backend: BD (block device with defragmentation)
// ---------------------------------------------------------------------------
static int backend_bd_add_file_fd(int fhi_fid, int fd, const char *path)
{
    int i, iop_fd;
    _off64_t size;
    unsigned int frag_start = 0;
    struct fhi_bd_file *frag = &g_bd->file[fhi_fid];

    (void)path;

    iop_fd = ps2sdk_get_iop_fd(fd);

    size = lseek64(fd, 0, SEEK_END);

    for (i = 0; i < FHI_MAX_FILES; i++)
        frag_start += g_bd->file[i].frag_count;

    frag->frag_start = frag_start;
    frag->frag_count = fileXioIoctl2(iop_fd, USBMASS_IOCTL_GET_FRAGLIST, NULL, 0,
                           (void *)&g_bd->frags[frag->frag_start],
                           sizeof(bd_fragment_t) * (BDM_MAX_FRAGS - frag->frag_start));
    frag->size = size;

    if ((frag->frag_start + frag->frag_count) > BDM_MAX_FRAGS) {
        printf("Too many fragments (%d)\n", frag->frag_start + frag->frag_count);
        return -1;
    }

    printf("file[%d] fragments: start=%u, count=%u\n", fhi_fid, frag->frag_start, frag->frag_count);
    for (i = 0; i < frag->frag_count; i++)
        printf("- frag[%d] start=%u, count=%u\n", i,
               (unsigned int)g_bd->frags[frag->frag_start + i].sector,
               g_bd->frags[frag->frag_start + i].count);

    g_bd->drvName = (uint32_t)fileXioIoctl2(iop_fd, USBMASS_IOCTL_GET_DRIVERNAME, NULL, 0, NULL, 0);
    fileXioIoctl2(iop_fd, USBMASS_IOCTL_GET_DEVICE_NUMBER, NULL, 0, &g_bd->devNr, 4);
    printf("Using BDM device: %s%d\n", (char *)&g_bd->drvName, (int)g_bd->devNr);

    return 0;
}

// ---------------------------------------------------------------------------
// Backend: FILEID (pre-opened file handles)
// Shared by both mmcefhi.irx and udpfs_fhi.irx
// ---------------------------------------------------------------------------
static int backend_fileid_add_file_fd(int fhi_fid, int fd, const char *path)
{
    int iop_fd = ps2sdk_get_iop_fd(fd);

    // Extract devNr from the digit immediately before ':' (e.g. "udpfs1:/..." -> devNr=1)
    if (path) {
        const char *colon = strchr(path, ':');
        if (colon != NULL && colon > path && *(colon - 1) >= '0' && *(colon - 1) <= '9')
            g_fileid->devNr = *(colon - 1) - '0';
    }

    g_fileid->file[fhi_fid].id   = fileXioIoctl2(iop_fd, 0x80, NULL, 0, NULL, 0);
    g_fileid->file[fhi_fid].size = lseek64(iop_fd, 0, SEEK_END);

    return 0;
}
