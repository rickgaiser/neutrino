#include <stdio.h>
#include <sysclib.h>
#include <loadcore.h>
#include <ioman.h>

#define MODNAME "img_driver"
IRX_ID(MODNAME, 1, 1);

struct SPatchData {
    unsigned int magic;
    unsigned int ioprpimg;
    int ioprpsiz;
} __attribute__((packed)) p = {0xDEC1DEC1, 0, 0};

static int dummy_fs()
{
    return 0;
}

static int lseek_fs(iop_file_t *fd, int offset, int whence)
{
    if (whence == SEEK_END) {
        return p.ioprpsiz;
    } else {
        return 0;
    }
}

static int read_fs(iop_file_t *fd, void *buffer, int size)
{
    memcpy(buffer, (void *)p.ioprpimg, size);
    return size;
}

static iop_device_ops_t my_device_ops =
{
    (void*)dummy_fs, // init
    (void*)dummy_fs, // deinit
    NULL,            // format
    (void*)dummy_fs, // open
    (void*)dummy_fs, // close
    read_fs,         // read
    NULL,            // write
    lseek_fs,        // lseek
    /*
    (void*)dummy_fs, // ioctl
    (void*)dummy_fs, // remove
    (void*)dummy_fs, // mkdir
    (void*)dummy_fs, // rmdir
    (void*)dummy_fs, // dopen
    (void*)dummy_fs, // dclose
    (void*)dummy_fs, // dread
    (void*)dummy_fs, // getstat
    (void*)dummy_fs, // chstat
    */
};

static const char name[] = "img";
static iop_device_t my_device = {
    name,
    IOP_DT_FS,
    1,
    name,
    &my_device_ops};

int _start(int argc, char **argv)
{
    // DelDrv("img");
    AddDrv((iop_device_t *)&my_device);

    return MODULE_RESIDENT_END;
}
