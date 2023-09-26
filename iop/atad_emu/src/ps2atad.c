#include <loadcore.h>
#include <stdio.h>
#include <string.h>
#include <atahw.h>

#include "atad.h"
#include "fhi.h"
#include "mprintf.h"

#define MODNAME "atad"
IRX_ID(MODNAME, 2, 7);

extern struct irx_export_table _exp_atad;

int _start(int argc, char *argv[])
{
    M_DEBUG("starting\n", "");

    if (RegisterLibraryEntries(&_exp_atad) != 0) {
        M_DEBUG("ERROR: library already registered\n");
        return MODULE_NO_RESIDENT_END;
    }

    return MODULE_RESIDENT_END;
}

int _exit(void) { return MODULE_RESIDENT_END; }

ata_devinfo_t dev[2] = {
    {0, 0, 0, 0},
    {0, 0, 0, 0}
};

/* Export 4 */
ata_devinfo_t *ata_get_devinfo(int device)
{
    M_DEBUG("(%d)\n", device);

    if (device == 0 && dev[0].exists == 0) {
        // Initialize
        u32 size = fhi_size(FHI_FID_ATA0);
        if (size > 0) {
            M_DEBUG("(%d) size = %d sectors\n", device, size);
            dev[0].exists          = 1;
            dev[0].has_packet      = 0;
            dev[0].total_sectors   = size;
            dev[0].security_status = ATA_F_SEC_ENABLED | ATA_F_SEC_LOCKED;
        }
    }

    return &dev[device];
}

/* Export 5 */
int ata_reset_devices(void)
{
    M_DEBUG("\n");
    return 0;
}

/* Export 6 */
int ata_io_start(void *buf, u32 blkcount, u16 feature, u16 nsector, u16 sector, u16 lcyl, u16 hcyl, u16 select, u16 command)
{
    M_DEBUG("(0x%x, %d, %d, %d, %d, %d, %d, %d, %d)\n", buf, blkcount, feature, nsector, sector, lcyl, hcyl, select, command);
    return 0;
}

/* Export 7 */
int ata_io_finish(void)
{
    M_DEBUG("\n");
    return 0;
}

/* Export 8 */
int ata_get_error(void)
{
    M_DEBUG("\n");
    return 0;
}

/* Export 9 */
int ata_device_sector_io(int device, void *buf, u32 lba, u32 nsectors, int dir)
{
    M_DEBUG("(%d, 0x%x, %d, %d, %d)\n", device, buf, lba, nsectors, dir);

    if (dir == ATA_DIR_WRITE) {
        fhi_write(FHI_FID_ATA0, buf, lba, nsectors);
        return 0;
    } else {
        fhi_read(FHI_FID_ATA0, buf, lba, nsectors);
        return 0;
    }

    return -1;
}

/* Export 10 */
int ata_device_sce_sec_set_password(int device, void *password)
{
    int i;

    M_DEBUG("(%d, password)\n", device);
    for (i=0; i<8; i++) {
        M_DEBUG("- 0x%08x\n", ((u32*)password)[i]);
    }

    return 0;
}

/* Export 11 */
int ata_device_sce_sec_unlock(int device, void *password)
{
    int i;

    M_DEBUG("(%d, password)\n", device);
    for (i=0; i<8; i++) {
        M_DEBUG("- 0x%08x\n", ((u32*)password)[i]);
    }

    dev[device].security_status &= ~ATA_F_SEC_LOCKED;

    return 0;
}

/* Export 12 */
int ata_device_sce_sec_erase(int device)
{
    M_DEBUG("(%d)\n", device);
    return 0;
}

/* Export 13 */
int ata_device_idle(int device, int period)
{
    M_DEBUG("(%d)\n", device);
    return 0;
}

/* Export 14 */
int ata_device_sce_identify_drive(int device, void *data)
{
    M_DEBUG("(%d)\n", device);

    // return zero filled response even if hdd id is not present
    memset(data, 0, 512);
    fhi_read(FHI_FID_ATA0ID, data, 0, 1);

    return 0;
}

/* Export 15 */
int ata_device_smart_get_status(int device)
{
    M_DEBUG("(%d)\n", device);
    return 0;
}

/* Export 16 */
int ata_device_smart_save_attr(int device)
{
    M_DEBUG("(%d)\n", device);
    return 0;
}

/* Export 17 */
int ata_device_flush_cache(int device)
{
    M_DEBUG("(%d)\n", device);
    return 0;
}

/* Export 18 */
int ata_device_idle_immediate(int device)
{
    M_DEBUG("(%d)\n", device);
    return 0;
}
