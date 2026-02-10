#include <loadcore.h>
#include <iomanX.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <hdd-ioctl.h>
#include <usbhdfsd-common.h>
#include "mprintf.h"

#define MODNAME "hdlfs"
IRX_ID(MODNAME, 1, 1);

typedef struct // size = 1024
{
    u32 checksum; // HDL uses 0xdeadfeed magic here
    u32 magic;
    char gamename[160];
    u8 hdl_compat_flags;
    u8 ops2l_compat_flags;
    u8 dma_type;
    u8 dma_mode;
    char startup[60];
    u32 layer1_start;
    u32 discType;
    int num_partitions;
    struct
    {
        u32 part_offset; // in 2048b sectors
        u32 data_start;  // in 512b sectors
        u32 part_size;   // in bytes
    } part_specs[65];
} hdl_apa_header;

#define HDL_GAME_DATA_OFFSET 0x100000 // Sector 0x800 in the extended attribute area.
#define HDL_FS_MAGIC         0x1337

hdl_apa_header file_hdl;
uint64_t file_offset = 0;
uint64_t file_size = 0;

int hdl_init(iomanX_iop_device_t *d)
{
    M_DEBUG("%s()\n", __FUNCTION__);

    int fd = iomanX_dopen("hdd0:");
    if (fd < 0) {
        M_DEBUG("WARNING: unable to open hdd0 at this time:\n");
        // Return succes so we can try to open hdd0 later
        return 0;//-ENODEV;
    }

#ifdef DEBUG
    iox_dirent_t dirent;
    M_DEBUG("List of APA partitions:\n", __FUNCTION__);
    while (iomanX_dread(fd, &dirent) > 0) {
        M_DEBUG("  %s, mode=0x%x, attr=0x%x, size=%d\n", dirent.name, dirent.stat.mode, dirent.stat.attr, dirent.stat.size);
    }
#endif

    iomanX_dclose(fd);
    return 0;
}
int hdl_deinit(iomanX_iop_device_t *d)
{
    M_DEBUG("%s()\n", __FUNCTION__);
    return 0;
}
int hdl_format(iomanX_iop_file_t *f, const char *unk1, const char *unk2, void *unk3, int unk4)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
int hdl_open(iomanX_iop_file_t *f, const char *name, int mode, int unk)
{
    iox_dirent_t dirent;

    M_DEBUG("%s(%s, 0x%x)\n", __FUNCTION__, name, mode);

    int fd = iomanX_dopen("hdd0:");
    if (fd < 0) {
        M_DEBUG("ERROR: unable to open hdd0:\n");
        return -ENODEV;
    }

    char *iso_ext = strstr(name, ".iso");
    if (iso_ext != NULL) {
        M_DEBUG("ignoring .iso extension\n");
        *iso_ext = '\0'; // Terminate the string
    }

    while (name[0] == '/' || name[0] == '\\') {
        M_DEBUG("ignoring leading '/'\n");
        name++;
    }

    while (iomanX_dread(fd, &dirent) > 0) {
        M_DEBUG("  %s, mode=0x%x, attr=0x%x, size=%d\n", dirent.name, dirent.stat.mode, dirent.stat.attr, dirent.stat.size);
        if (dirent.stat.mode == HDL_FS_MAGIC && (dirent.stat.attr & APA_FLAG_SUB) == 0) {
            int i;

            // Note: The APA specification states that there is a 4KB area used for storing the partition's information, before the extended attribute area.
            unsigned int lba = dirent.stat.private_5 + (HDL_GAME_DATA_OFFSET + 4096) / 512;
            unsigned int nsectors = 2; // 2 * 512 = 1024 byte

            // Read HDLoader header
            hddAtaTransfer_t *args = (hddAtaTransfer_t *)&file_hdl;
            args->lba = lba;
            args->size = nsectors;
            if (iomanX_devctl("hdd0:", HDIOC_READSECTOR, args, sizeof(hddAtaTransfer_t), &file_hdl, nsectors * 512) != 0)
                return -EIO;

            if (file_hdl.checksum != 0xdeadfeed) {
                M_DEBUG("- HDL checksum invalid (0x%X, p5=0x%X)\n", file_hdl.checksum, dirent.stat.private_5);
                continue;
            }

            M_DEBUG("- name = %s\n", file_hdl.gamename);
            M_DEBUG("- partitions:\n");
            // Get game fragments
            for (i = 0; i < file_hdl.num_partitions; i++) {
                M_DEBUG("  - part[%d] dstart=%04uMiB, poffset=%04uMiB, psize=%04uMiB\n", i, file_hdl.part_specs[i].data_start / 2048, file_hdl.part_specs[i].part_offset / 512, file_hdl.part_specs[i].part_size / (1024*1024));
            }

            if ((strcmp(file_hdl.gamename, name) == 0) || (strcmp(dirent.name, name) == 0)) {
                // "open" the file
                file_offset = 0;
                file_size = 0;

                // Get file size
                for (i = 0; i < file_hdl.num_partitions; i++) {
                    file_size += file_hdl.part_specs[i].part_size;
                }

                M_DEBUG("\n\n!!! Game found (%dMiB / %uB) !!!\n\n\n", (uint32_t)(file_size / (1024*1024)), (uint32_t)file_size);

                iomanX_dclose(fd);
                return 1; // the 1 and only file descriptor, for now...
            }
        }
    }

    iomanX_dclose(fd);
    return -EIO;
}
int hdl_close(iomanX_iop_file_t *f)
{
    M_DEBUG("%s()\n", __FUNCTION__);
    return 0;
}
unsigned int get_part(u32 sector)
{
    unsigned int i;
    u32 lsn = sector / 4;

    for (i = 0; i < file_hdl.num_partitions; i++) {
        if ((lsn >= file_hdl.part_specs[i].part_offset) && (lsn < (file_hdl.part_specs[i].part_offset + (file_hdl.part_specs[i].part_size / 2048)))) {
            return i;
        }
    }

    return 0;
}
u8 sector_buffer[512];
int hdl_read(iomanX_iop_file_t *f, void *buffer, int size)
{
    M_DEBUG("%s(0x%x, %d)\n", __FUNCTION__, buffer, size);

    hddAtaTransfer_t *args = (hddAtaTransfer_t *)&sector_buffer;
    int size_left = size;

    while (size_left > 0) {
        unsigned int file_sector = file_offset / 512;
        unsigned int file_sector_offset = file_offset & 511;
        unsigned int part = get_part(file_sector);
        unsigned int part_sector = file_sector - (file_hdl.part_specs[part].part_offset * 4);
        unsigned int hdd_sector = file_hdl.part_specs[part].data_start + part_sector;
        int size_read = size_left;

        if (size_read > (512 - file_sector_offset))
            size_read = 512 - file_sector_offset;

        M_DEBUG("reading %d bytes from lba %d, offset %d\n", size_read, hdd_sector, file_sector_offset);

        args->lba = hdd_sector;
        args->size = 1;
        if (size_read == 512) {
            // Read whole sector
            if (iomanX_devctl("hdd0:", HDIOC_READSECTOR, args, sizeof(hddAtaTransfer_t), buffer, 512) != 0)
                return -EIO;
        }
        else {
            // Read part of sector via bounce buffer
            if (iomanX_devctl("hdd0:", HDIOC_READSECTOR, args, sizeof(hddAtaTransfer_t), &sector_buffer, 512) != 0)
                return -EIO;
            memcpy(buffer, sector_buffer + file_sector_offset, size_read);
        }

        buffer += size_read;
        size_left -= size_read;
        file_offset += size_read;
    }
    return size;
}
int hdl_write(iomanX_iop_file_t *f, void *buffer, int size)
{
    M_DEBUG("%s(0x%x, %d) - not supported\n", __FUNCTION__, buffer, size);
    return -EIO;
}
s64	hdl_lseek64(iomanX_iop_file_t *, s64 offset, int whence);
int hdl_lseek(iomanX_iop_file_t *f, int offset, int whence)
{
    M_DEBUG("%s(%d, %d)\n", __FUNCTION__, offset, whence);
    return hdl_lseek64(f, offset, whence);
}
int hdl_ioctl(iomanX_iop_file_t *f, int, void *)
{
    M_DEBUG("%s()\n", __FUNCTION__);
    return -EIO;
}
int hdl_remove(iomanX_iop_file_t *f, const char *)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
int hdl_mkdir(iomanX_iop_file_t *f, const char *path, int unk)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
int hdl_rmdir(iomanX_iop_file_t *f, const char *path)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
int hdl_dopen(iomanX_iop_file_t *f, const char *path)
{
    M_DEBUG("%s()\n", __FUNCTION__);
    return -EIO;
}
int hdl_dclose(iomanX_iop_file_t *f)
{
    M_DEBUG("%s()\n", __FUNCTION__);
    return -EIO;
}
int hdl_dread(iomanX_iop_file_t *f, iox_dirent_t *dirent)
{
    M_DEBUG("%s()\n", __FUNCTION__);
    return -EIO;
}
int hdl_getstat(iomanX_iop_file_t *f, const char *name, iox_stat_t *stat)
{
    M_DEBUG("%s()\n", __FUNCTION__);
    return -EIO;
}
int hdl_chstat(iomanX_iop_file_t *f, const char *name, iox_stat_t *stat, unsigned int)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
/* Extended ops start here.  */
int	hdl_rename(iomanX_iop_file_t *, const char *, const char *)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
int	hdl_chdir(iomanX_iop_file_t *, const char *)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
int	hdl_sync(iomanX_iop_file_t *, const char *, int)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
int	hdl_mount(iomanX_iop_file_t *, const char *, const char *, int, void *, int)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
int	hdl_umount(iomanX_iop_file_t *, const char *)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
s64	hdl_lseek64(iomanX_iop_file_t *, s64 offset, int whence)
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
int	hdl_devctl(iomanX_iop_file_t *, const char *, int, void *, unsigned int, void *, unsigned int)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
int	hdl_symlink(iomanX_iop_file_t *, const char *, const char *)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
int	hdl_readlink(iomanX_iop_file_t *, const char *, char *, unsigned int)
{
    M_DEBUG("%s() - not supported\n", __FUNCTION__);
    return -EIO;
}
static int get_frag_list(void *rdata, unsigned int rdatalen)
{
    bd_fragment_t *f = (bd_fragment_t*)rdata;
    int iMaxFragments = rdatalen / sizeof(bd_fragment_t);
    int i;

    if (iMaxFragments < file_hdl.num_partitions) {
        return -EINVAL;
    }

    for (i = 0; i < file_hdl.num_partitions; i++) {
        f[i].sector = file_hdl.part_specs[i].data_start;
        f[i].count  = file_hdl.part_specs[i].part_size / 512;
    }

    return file_hdl.num_partitions;
}
int	hdl_ioctl2(iomanX_iop_file_t *, int cmd, void *data, unsigned int datalen, void *rdata, unsigned int rdatalen)
{
    int ret = -EINVAL;

    M_DEBUG("%s(%d)\n", __FUNCTION__, cmd);

    switch (cmd) {
        case USBMASS_IOCTL_GET_DRIVERNAME:
            ret = *(int *)"ata";
            break;
        case USBMASS_IOCTL_GET_FRAGLIST:
            ret = get_frag_list(rdata, rdatalen);
            break;
        case USBMASS_IOCTL_GET_DEVICE_NUMBER:
        {
            // Check for a return buffer and copy the device number. If no buffer is provided return an error.
            if (rdata == NULL || rdatalen < sizeof(u32))
            {
                ret = -EINVAL;
                break;
            }
            *(u32*)rdata = 0;
            ret = 0;
            break;
        }
    }

    return ret;
}

iomanX_iop_device_ops_t hdl_device_ops = {
    hdl_init,
    hdl_deinit,
    hdl_format,
    hdl_open,
    hdl_close,
    hdl_read,
    hdl_write,
    hdl_lseek,
    hdl_ioctl,
    hdl_remove,
    hdl_mkdir,
    hdl_rmdir,
    hdl_dopen,
    hdl_dclose,
    hdl_dread,
    hdl_getstat,
    hdl_chstat,
    hdl_rename,
    hdl_chdir,
    hdl_sync,
    hdl_mount,
    hdl_umount,
    hdl_lseek64,
    hdl_devctl,
    hdl_symlink,
    hdl_readlink,
    hdl_ioctl2
};

const char hdl_fs_name[] = "hdl";
iomanX_iop_device_t hdl_device = {
    hdl_fs_name,
    IOP_DT_FSEXT | IOP_DT_FS,
    1,
    hdl_fs_name,
    &hdl_device_ops};

int _start(int argc, char *argv[])
{
    M_DEBUG("HDLoader read-only filesystem driver\n");

    if(AddDrv(&hdl_device) != 0) { return MODULE_NO_RESIDENT_END; }
    return MODULE_RESIDENT_END;
}
