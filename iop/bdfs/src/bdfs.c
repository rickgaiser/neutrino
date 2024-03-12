#include <loadcore.h>
#include <iomanX.h>
#include <errno.h>
#include <string.h>
#include <sysclib.h>
#include <stdint.h>
#include <bdm.h>
#include <usbhdfsd-common.h>
#include "mprintf.h"

#define MODNAME "bdfs"
IRX_ID(MODNAME, 1, 1);

uint64_t file_offset = 0;
uint64_t file_size = 0;
struct block_device *bd = NULL;

int bdfs_init(iomanX_iop_device_t *d)
{
    M_DEBUG("%s()\n", __FUNCTION__);
    // Return succes
    return 0;
}
int bdfs_deinit(iomanX_iop_device_t *d)
{
    M_DEBUG("%s()\n", __FUNCTION__);
    return 0;
}
int bdfs_format(iomanX_iop_file_t *f, const char *unk1, const char *unk2, void *unk3, int unk4)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
int bdfs_open(iomanX_iop_file_t *f, const char *name, int mode, int unk)
{
    struct block_device *pbd[10];
    char bdname[10];
    int i;

    M_DEBUG("%s(%s, 0x%x)\n", __FUNCTION__, name, mode);

    while (name[0] == '/' || name[0] == '\\') {
        M_DEBUG("ignoring leading '/'\n");
        name++;
    }

    file_offset = 0;
    file_size = 0;
    bd = NULL;

    bdm_get_bd(pbd, 10);
    for (i = 0; i < 10; i++) {
        sprintf(bdname, "%s%dp%d", pbd[i]->name, pbd[i]->devNr, pbd[i]->parNr);
        M_DEBUG("- BD[%d] = %s\n", i, bdname);
        if (strcmp(bdname, name) == 0) {
            bd = pbd[i];
            file_size = pbd[i]->sectorCount * 512;
            return 1; // the 1 and only file descriptor, for now...
        }
    }

    return -EIO;
}
int bdfs_close(iomanX_iop_file_t *f)
{
    M_DEBUG("%s()\n", __FUNCTION__);
    return 0;
}

u8 sector_buffer[512];
int bdfs_read(iomanX_iop_file_t *f, void *buffer, int size)
{
    M_DEBUG("%s(0x%x, %d)\n", __FUNCTION__, buffer, size);

    int size_left = size;
    while (size_left > 0) {
        unsigned int file_sector = file_offset / 512;
        unsigned int file_sector_offset = file_offset & 511;
        int size_read = size_left;

        if (size_read > (512 - file_sector_offset))
            size_read = 512 - file_sector_offset;

        M_DEBUG("reading %d bytes from lba %d, offset %d\n", size_read, file_sector, file_sector_offset);

        if (size_read == 512) {
            // Read whole sector
            if (bd->read(bd, file_sector, buffer, 1) != 1)
                return -EIO;
        }
        else {
            // Read part of sector via bounce buffer
            if (bd->read(bd, file_sector, sector_buffer, 1) != 1)
                return -EIO;
            memcpy(buffer, sector_buffer + file_sector_offset, size_read);
        }

        buffer += size_read;
        size_left -= size_read;
        file_offset += size_read;
    }
    return size;
}
int bdfs_write(iomanX_iop_file_t *f, void *buffer, int size)
{
    M_DEBUG("%s(0x%x, %d) - not supported\n", __FUNCTION__, buffer, size);
    return -EIO;
}
s64	bdfs_lseek64(iomanX_iop_file_t *, s64 offset, int whence);
int bdfs_lseek(iomanX_iop_file_t *f, int offset, int whence)
{
    M_DEBUG("%s(%d, %d)\n", __FUNCTION__, offset, whence);
    return bdfs_lseek64(f, offset, whence);
}
int bdfs_ioctl(iomanX_iop_file_t *f, int, void *)
{
    M_DEBUG("%s()\n", __FUNCTION__);
    return -EIO;
}
int bdfs_remove(iomanX_iop_file_t *f, const char *)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
int bdfs_mkdir(iomanX_iop_file_t *f, const char *path, int unk)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
int bdfs_rmdir(iomanX_iop_file_t *f, const char *path)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
int bdfs_dopen(iomanX_iop_file_t *f, const char *path)
{
    M_DEBUG("%s()\n", __FUNCTION__);
    return -EIO;
}
int bdfs_dclose(iomanX_iop_file_t *f)
{
    M_DEBUG("%s()\n", __FUNCTION__);
    return -EIO;
}
int bdfs_dread(iomanX_iop_file_t *f, iox_dirent_t *dirent)
{
    M_DEBUG("%s()\n", __FUNCTION__);
    return -EIO;
}
int bdfs_getstat(iomanX_iop_file_t *f, const char *name, iox_stat_t *stat)
{
    M_DEBUG("%s()\n", __FUNCTION__);
    return -EIO;
}
int bdfs_chstat(iomanX_iop_file_t *f, const char *name, iox_stat_t *stat, unsigned int)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
/* Extended ops start here.  */
int	bdfs_rename(iomanX_iop_file_t *, const char *, const char *)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
int	bdfs_chdir(iomanX_iop_file_t *, const char *)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
int	bdfs_sync(iomanX_iop_file_t *, const char *, int)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
int	bdfs_mount(iomanX_iop_file_t *, const char *, const char *, int, void *, int)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
int	bdfs_umount(iomanX_iop_file_t *, const char *)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
s64	bdfs_lseek64(iomanX_iop_file_t *, s64 offset, int whence)
{
    M_DEBUG("%s(%d, %d)\n", __FUNCTION__, offset, whence);

    switch (whence) {
        case SEEK_SET: file_offset  = offset; break;
        case SEEK_CUR: file_offset += offset; break;
        case SEEK_END: file_offset  = file_size + offset; break;
        default:
            return -1;
    }

    return file_offset;
}
int	bdfs_devctl(iomanX_iop_file_t *, const char *, int, void *, unsigned int, void *, unsigned int)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
int	bdfs_symlink(iomanX_iop_file_t *, const char *, const char *)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
int	bdfs_readlink(iomanX_iop_file_t *, const char *, char *, unsigned int)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
int	bdfs_ioctl2(iomanX_iop_file_t *, int cmd, void *data, unsigned int datalen, void *rdata, unsigned int rdatalen)
{
    int ret = -EINVAL;

    M_DEBUG("%s(%d)\n", __FUNCTION__, cmd);

    switch (cmd) {
        case USBMASS_IOCTL_GET_DRIVERNAME:
            ret = *(int *)bd->name;
            break;
        case USBMASS_IOCTL_GET_FRAGLIST:
            // Check for a return buffer and copy the device number. If no buffer is provided return an error.
            if (rdata == NULL || rdatalen < sizeof(bd_fragment_t))
            {
                ret = -EINVAL;
                break;
            }
            // Block device always has 1 fragment
            {
                bd_fragment_t *f = (bd_fragment_t*)rdata;
                f[0].sector = bd->sectorOffset;
                f[0].count  = bd->sectorCount;
            }
            ret = 1;
            break;
        case USBMASS_IOCTL_GET_DEVICE_NUMBER:
        {
            // Check for a return buffer and copy the device number. If no buffer is provided return an error.
            if (rdata == NULL || rdatalen < sizeof(u32))
            {
                ret = -EINVAL;
                break;
            }
            *(u32*)rdata = bd->devNr;
            ret = 0;
            break;
        }
    }

    return ret;
}

iomanX_iop_device_ops_t bdfs_device_ops = {
    bdfs_init,
    bdfs_deinit,
    bdfs_format,
    bdfs_open,
    bdfs_close,
    bdfs_read,
    bdfs_write,
    bdfs_lseek,
    bdfs_ioctl,
    bdfs_remove,
    bdfs_mkdir,
    bdfs_rmdir,
    bdfs_dopen,
    bdfs_dclose,
    bdfs_dread,
    bdfs_getstat,
    bdfs_chstat,
    bdfs_rename,
    bdfs_chdir,
    bdfs_sync,
    bdfs_mount,
    bdfs_umount,
    bdfs_lseek64,
    bdfs_devctl,
    bdfs_symlink,
    bdfs_readlink,
    bdfs_ioctl2
};

const char bdfs_fs_name[] = "bdfs";
iomanX_iop_device_t bdfs_device = {
    bdfs_fs_name,
    IOP_DT_FSEXT | IOP_DT_FS,
    1,
    bdfs_fs_name,
    &bdfs_device_ops};

int _start(int argc, char *argv[])
{
    M_DEBUG("Block Device read-only filesystem driver\n");

    AddDrv(&bdfs_device);

    return MODULE_RESIDENT_END;
}
