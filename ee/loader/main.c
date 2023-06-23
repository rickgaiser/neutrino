// libc/newlib
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>

// PS2SDK
#include <kernel.h>
#include <tamtypes.h>
#include <loadfile.h>
#include <iopcontrol.h>
#include <usbhdfsd-common.h>
#include <libcdvd-common.h>

// Other
#include "elf.h"
#include "patch.h"
#include "modules.h"
#include "ee_core_config.h"
#include "ioprp.h"
#include "../../iop/common/cdvd_config.h"

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <io_common.h>


#define OPL_MOD_STORAGE 0x00097000 //(default) Address of the module storage region
/*
OPL Module Storage Memory Map:
    struct irxtab_t;
    struct irxptr_tab[modcount];
    udnl.irx
    IOPRP.img, containing:
    - cdvdman.irx
    - cdvdfsv.irx
    - eesync.irx
    imgdrv.irx
    resetspu.irx
    <extra modules>
*/

static struct SEECoreConfig eeconf;

static const struct cdvdman_settings_common cdvdman_settings_common_sample = {
    0x69, 0x69, 0x1234, 0x39393939, {'B', '0', '0', 'B', 'S'}};

void print_usage()
{
    printf("ps2client -h 192.168.1.10 execee host:neutrino.elf <driver> <path>\n");
    printf("Supported drivers:\n");
    printf(" - ata\n");
    printf(" - usb\n");
    printf(" - mx4sio\n");
    printf(" - udpbd\n");
    printf(" - ilink\n");
    printf("\n");
    printf("Usage example:\n");
    printf("  ps2client -h 192.168.1.10 execee host:neutrino.elf usb mass:path/to/filename.iso\n");
}

struct SModule
{
    const char *sUDNL;
    const char *sFileName;
    void *pData;
    off_t iSize;
    bool bLoaded;
    u32 iFlags;
    u32 eecid;
};
#define SMF_IOPCORE  (1 << 2)
#define SMF_SYSTEM   (1 << 3)
#define SMF_D_ATA    (1 << 10)
#define SMF_D_USB    (1 << 11)
#define SMF_D_UDPBD  (1 << 12)
#define SMF_D_MX4SIO (1 << 13)
#define SMF_D_ILINK  (1 << 14)
// clang-format off
struct SModule mod[] = {
    {"",        "udnl.irx"             , NULL, 0, false, SMF_IOPCORE  , OPL_MODULE_ID_UDNL},
    {"CDVDMAN", "bdm_cdvdman.irx"      , NULL, 0, false, SMF_IOPCORE  , 0},
    {"CDVDFSV", "cdvdfsv.irx"          , NULL, 0, false, SMF_IOPCORE  , 0},
    {"EESYNC",  "eesync.irx"           , NULL, 0, false, SMF_IOPCORE  , 0},
    {"",        "imgdrv.irx"           , NULL, 0, false, SMF_IOPCORE  , OPL_MODULE_ID_IMGDRV},
    {"",        "resetspu.irx"         , NULL, 0, false, SMF_IOPCORE  , OPL_MODULE_ID_RESETSPU},
    {"",        "iomanX.irx"           , NULL, 0, false, SMF_SYSTEM   , 0},
    {"",        "fileXio.irx"          , NULL, 0, false, SMF_SYSTEM   , 0},
    {"",        "isofs.irx"            , NULL, 0, false, SMF_SYSTEM   , 0},
    {"",        "bdm.irx"              , NULL, 0, false, SMF_SYSTEM   , 0},
    {"",        "bdmfs_fatfs.irx"      , NULL, 0, false, SMF_SYSTEM   , 0},
    {"",        "ata_bd.irx"           , NULL, 0, false, SMF_D_ATA    , 0},
    {"",        "usbd_mini.irx"        , NULL, 0, false, SMF_D_USB    , 0},
    {"",        "usbmass_bd_mini.irx"  , NULL, 0, false, SMF_D_USB    , 0},
    {"",        "mx4sio_bd_mini.irx"   , NULL, 0, false, SMF_D_MX4SIO , 0},
    {"",        "ps2dev9.irx"          , NULL, 0, false, SMF_D_UDPBD  , 0},
    {"",        "smap.irx"             , NULL, 0, false, SMF_D_UDPBD  , 0},
    {"",        "iLinkman.irx"         , NULL, 0, false, SMF_D_ILINK  , 0},
    {"",        "IEEE1394_bd_mini.irx" , NULL, 0, false, SMF_D_ILINK  , 0},
    {NULL, NULL, 0, 0}
};
// clang-format on
struct SModule *mod_cdvdman = &mod[1];

#define MAX_FILENAME 128
int load_module(struct SModule *mod)
{
    char sFilePath[MAX_FILENAME];

    //printf("%s(%s)\n", __FUNCTION__, mod->sFileName);

    if (mod->bLoaded == true) {
        //printf("WARNING: Module already loaded: %s\n", mod->sFileName);
        return 0;
    }

    // Open module on default location
    snprintf(sFilePath, MAX_FILENAME, "modules/%s", mod->sFileName);
    int fd = open(sFilePath, O_RDONLY);
    if (fd < 0) {
        printf("ERROR: Unable to open %s\n", mod->sFileName);
        return -1;
    }

    //printf("%s(%s) loaded %s\n", __FUNCTION__, mod->sFileName, sFilePath);

    // Get module size
    mod->iSize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    // Allocate memory for module
    mod->pData = malloc(mod->iSize);

    // Load module into memory
    read(fd, mod->pData, mod->iSize);

    // Close module
    close(fd);

    mod->bLoaded = true;
    return 0;
}

struct SModule *load_module_name(const char *name)
{
    int modid;

    for (modid = 0; mod[modid].sFileName != NULL; modid++) {
        if (strcmp(mod[modid].sFileName, name) == 0) {
            load_module(&mod[modid]);
            return &mod[modid];
        }
    }

    return NULL;
}

static off_t load_file_mod(const char *filename, void *addr, irxptr_t *irx)
{
    struct SModule *mod = load_module_name(filename);
    if (mod == NULL) {
        printf("ERROR: cannot find %s\n", filename);
        return 0;
    }

    memcpy(addr, mod->pData, mod->iSize);
    irx->info = mod->iSize | SET_OPL_MOD_ID(mod->eecid);
    irx->ptr = addr;

    printf("SYSTEM IRX %u address start: %p end: %p\n", mod->eecid, addr, (u8*)addr + mod->iSize);

    // Align to 16 bytes
    return (mod->iSize + 0xf) & ~0xf;
}

struct SModule *load_module_udnlname(const char *name)
{
    int modid;

    for (modid = 0; mod[modid].sFileName != NULL; modid++) {
        if (strcmp(mod[modid].sUDNL, name) == 0) {
            load_module(&mod[modid]);
            return &mod[modid];
        }
    }

    return NULL;
}

int start_module(struct SModule *mod)
{
    if (mod->bLoaded == false) {
        int rv = load_module(mod);
        if (rv < 0)
            return rv;
    }

    if (SifExecModuleBuffer(mod->pData, mod->iSize, 0, NULL, NULL) < 0) {
        printf("ERROR: Could not load %s\n", mod->sFileName);
        return -1;
    }

    printf("- %s started\n", mod->sFileName);

    return 0;
}

int load_modules(u32 flags)
{
    int modid;

    for (modid = 0; mod[modid].sFileName != NULL; modid++) {
        if (mod[modid].iFlags & flags) {
            if(load_module(&mod[modid]) < 0)
                return -1;
        }
    }
    return 0;
}

int start_modules(u32 flags)
{
    int modid;

    for (modid = 0; mod[modid].sFileName != NULL; modid++) {
        if (mod[modid].iFlags & flags) {
            if (start_module(&mod[modid]) < 0)
                return -1;
        }
    }
    return 0;
}

/*----------------------------------------------------------------------------------------
    Replace modules in IOPRP image:
    - CDVDMAN
    - CDVDFSV
    - EESYNC
------------------------------------------------------------------------------------------*/
static unsigned int patch_IOPRP_image(struct romdir_entry *romdir_out, const struct romdir_entry *romdir_in)
{
    struct romdir_entry *romdir_out_org = romdir_out;
    u8 *ioprp_in = (u8 *)romdir_in;
    u8 *ioprp_out = (u8 *)romdir_out;

    while (romdir_in->name[0] != '\0') {
        struct SModule *mod = load_module_udnlname(romdir_in->name);
        if (mod != NULL) {
            //printf("IOPRP: replacing %s with %s\n", romdir_in->name, mod->sFileName);
            memcpy(ioprp_out, mod->pData, mod->iSize);
            romdir_out->size = mod->iSize;
        } else {
            //printf("IOPRP: keeping %s\n", romdir_in->name);
            memcpy(ioprp_out, ioprp_in, romdir_in->size);
            romdir_out->size = romdir_in->size;
        }

        // Align all addresses to a multiple of 16
        ioprp_in += (romdir_in->size + 0xF) & ~0xF;
        ioprp_out += (romdir_out->size + 0xF) & ~0xF;
        romdir_in++;
        romdir_out++;
    }

    return (ioprp_out - (u8 *)romdir_out_org);
}

struct ioprp_ext {
    extinfo_t reset_date_ext;
    uint32_t  reset_date;

    extinfo_t cdvdman_date_ext;
    uint32_t  cdvdman_date;
    extinfo_t cdvdman_version_ext;
    extinfo_t cdvdman_comment_ext;
    char      cdvdman_comment[12];

    extinfo_t cdvdfsv_date_ext;
    uint32_t  cdvdfsv_date;
    extinfo_t cdvdfsv_version_ext;
    extinfo_t cdvdfsv_comment_ext;
    char      cdvdfsv_comment[16];

    extinfo_t syncee_date_ext;
    uint32_t  syncee_date;
    extinfo_t syncee_version_ext;
    extinfo_t syncee_comment_ext;
    char      syncee_comment[8];
};

#define ROMDIR_ENTRY_COUNT 7
struct ioprp_img
{
    romdir_entry_t romdir[ROMDIR_ENTRY_COUNT];
    struct ioprp_ext ext;
};

static const struct ioprp_img ioprp_img_base = {
    {{"RESET"  ,  8, 0},
     {"ROMDIR" ,  0, 0x10 * ROMDIR_ENTRY_COUNT},
     {"EXTINFO",  0, sizeof(struct ioprp_ext)},
     {"CDVDMAN", 28, 0},
     {"CDVDFSV", 32, 0},
     {"EESYNC" , 24, 0},
     {"", 0, 0}},
    {
        // RESET extinfo
        {0, 4, EXTINFO_TYPE_DATE},
        0x20230621,
        // CDVDMAN extinfo
        {0, 4, EXTINFO_TYPE_DATE},
        0x20230621,
        {0x9999, 0, EXTINFO_TYPE_VERSION},
        {0, 12, EXTINFO_TYPE_COMMENT},
        "cdvd_driver",
        // CDVDFSV extinfo
        {0, 4, EXTINFO_TYPE_DATE},
        0x20230621,
        {0x9999, 0, EXTINFO_TYPE_VERSION},
        {0, 16, EXTINFO_TYPE_COMMENT},
        "cdvd_ee_driver",
        // SYNCEE extinfo
        {0, 4, EXTINFO_TYPE_DATE},
        0x20230621,
        {0x9999, 0, EXTINFO_TYPE_VERSION},
        {0, 8, EXTINFO_TYPE_COMMENT},
        "SyncEE"
    }};


int main(int argc, char *argv[])
{
    irxtab_t *irxtable;
    irxptr_t *irxptr_tab;
    u8 *irxptr;
    int fd;
    int i;
    off_t size;
    void *eeloadCopy, *initUserMemory;
    struct cdvdman_settings_bdm *settings;
    const char *sConfigName;
    int iMode;
    int iDrivers;

    printf("----------------------------\n");
    printf("- Neutrino PS2 Game Loader -\n");
    printf("-       By Maximus32       -\n");
    printf("----------------------------\n");

    // printf("argc = %d\n", argc);
    // for (int i=0; i<argc; i++)
    //     printf("argv[%d] = %s\n", i, argv[i]);

    if (argc != 3) {
        printf("ERROR: argc = %d, should be 3\n", argc);
        print_usage();
        return -1;
    }

    /*
     * Figure out what drivers we need
     */
    const char *sDriver = argv[1];
    if (!strncmp(sDriver, "ata", 3)) {
        printf("Loading ATA drivers\n");
        iMode = BDM_ATA_MODE;
        iDrivers = SMF_D_ATA;
    } else if (!strncmp(sDriver, "usb", 3)) {
        printf("Loading USB drivers\n");
        iMode = BDM_USB_MODE;
        iDrivers = SMF_D_USB;
    } else if (!strncmp(sDriver, "udpbd", 5)) {
        printf("Loading UDPBD drivers\n");
        iMode = BDM_UDP_MODE;
        iDrivers = SMF_D_UDPBD;
    } else if (!strncmp(sDriver, "mx4sio", 6)) {
        printf("Loading MX4SIO drivers\n");
        iMode = BDM_M4S_MODE;
        iDrivers = SMF_D_MX4SIO;
    } else if (!strncmp(sDriver, "ilink", 5)) {
        printf("Loading iLink drivers\n");
        iMode = BDM_ILK_MODE;
        iDrivers = SMF_D_ILINK;
    } else {
        printf("ERROR: driver %s not supported\n", sDriver);
        print_usage();
        return -1;
    }

    /*
     * Load drivers before rebooting the IOP
     */
    if (load_modules(SMF_SYSTEM|SMF_IOPCORE|iDrivers) < 0)
        return -1;

    /*
     * Load EECORE before rebooting the IOP
     */
    fd = open("modules/ee_core.elf", O_RDONLY);
    if (fd < 0) {
        printf("Unable to open %s\n", "modules/ee_core.elf");
        return -1;
    }
    size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    void *ee_core = malloc(size);
    read(fd, ee_core, size);
    close(fd);

    /*
     * Reboot the IOP
     */
    //fileXioExit();
    SifExitIopHeap();
    SifLoadFileExit();
    SifExitRpc();
    SifInitRpc(0);
    while(!SifIopReset(NULL, 0)){};
    while(!SifIopSync()) {};
    SifInitRpc(0);
    SifInitIopHeap();
    SifLoadFileInit();
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();

    /*
     * Load system drivers
     */
    if (start_modules(SMF_SYSTEM|iDrivers))
        return -1;
    fileXioInit();

    /*
     * Check if file exists
     * Give low level drivers 10s to start
     */
    const char *sFileName = argv[2];
    printf("Loading %s...\n", sFileName);
    for (i = 0; i < 10; i++) {
        fd = open(sFileName, O_RDONLY);
        if (fd >= 0)
            break;

        // Give low level drivers some time to init
        sleep(1);
    }
    if (fd < 0) {
        printf("Unable to open %s\n", sFileName);
        return -1;
    }
    // Get ISO file size
    size = lseek64(fd, 0, SEEK_END);
    char buffer[6];
    // Validate this is an ISO
    lseek64(fd, 16 * 2048, SEEK_SET);
    if (read(fd, buffer, sizeof(buffer)) != sizeof(buffer)) {
        printf("Unable to read ISO\n");
        return -1;
    }
    if ((buffer[0x00] != 1) || (strncmp(&buffer[0x01], "CD001", 5))) {
        printf("File is not a valid ISO\n");
        return -1;
    }
    // Get ISO layer0 size
    uint32_t layer0_lba_size;
    lseek64(fd, 16 * 2048 + 80, SEEK_SET);
    if (read(fd, &layer0_lba_size, sizeof(layer0_lba_size)) != sizeof(layer0_lba_size)) {
        printf("ISO invalid\n");
        return -1;
    }
    // Try to get ISO layer1 size
    uint32_t layer1_lba_start = 0;
    lseek64(fd, (uint64_t)layer0_lba_size * 2048, SEEK_SET);
    if (read(fd, buffer, sizeof(buffer)) == sizeof(buffer)) {
        if ((buffer[0x00] == 1) && (!strncmp(&buffer[0x01], "CD001", 5))) {
            layer1_lba_start = layer0_lba_size - 16;
            printf("- DVD-DL detected\n");
        }
    }
    printf("- size = %lldMiB\n", size / (1024 * 1024));

    /*
     * Mount as ISO so we can get some information
     */
    int fd_isomount = fileXioMount("iso:", sFileName, FIO_MT_RDONLY);
    if (fd_isomount < 0) {
        printf("ERROR: Unable to mount %s as iso\n", sFileName);
        return -1;
    }
    int fd_config = open("iso:/SYSTEM.CNF;1", O_RDONLY);
    if (fd_config < 0) {
        printf("ERROR: Unable to open %s\n", "iso:/SYSTEM.CNF;1");
        return -1;
    }
    char config_data[128];
    read(fd_config, config_data, 128);
    char *fname_start = strstr(config_data, "cdrom0:");
    char *fname_end = strstr(config_data, ";1");
    if (fname_start == NULL || fname_end == NULL) {
        printf("ERROR: file name not found in SYSTEM.CNF\n");
        return -1;
    }
    sConfigName = &fname_start[8];
    fname_end[0] = '\0';
    printf("config name: %s\n", sConfigName);
    close(fd_config);
    fileXioUmount("iso:");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
    memset((void *)0x00084000, 0, 0x00100000 - 0x00084000);
#pragma GCC diagnostic pop

    irxtable = (irxtab_t *)OPL_MOD_STORAGE;
    irxptr_tab = (irxptr_t *)((unsigned char *)irxtable + sizeof(irxtab_t));
    irxptr = (u8 *)((((unsigned int)irxptr_tab + sizeof(irxptr_t) * 20 /* MAX number of modules !!! */) + 0xF) & ~0xF);

    irxtable->modules = irxptr_tab;
    irxtable->count = 0;

    //
    // Load udnl.irx first
    //
    irxptr += load_file_mod("udnl.irx", irxptr, irxptr_tab++);
    irxtable->count++;

    //
    // Locate and set cdvdman settings
    //
    load_module(mod_cdvdman);
    for (i = 0, settings = NULL; i < mod_cdvdman->iSize; i += 4) {
        if (!memcmp((void *)((u8 *)mod_cdvdman->pData + i), &cdvdman_settings_common_sample, sizeof(cdvdman_settings_common_sample))) {
            settings = (struct cdvdman_settings_bdm *)((u8 *)mod_cdvdman->pData + i);
            break;
        }
    }
    if (i >= mod_cdvdman->iSize) {
        printf("ERROR: unable to locate cdvdman settings\n");
        return -1;
    }
    memset((void *)settings, 0, sizeof(struct cdvdman_settings_bdm));
    settings->common.NumParts = 1;
    settings->common.media = SCECdPS2DVD;
    //settings->common.flags = IOPCORE_COMPAT_ACCU_READS;
    settings->common.layer1_start = layer1_lba_start;
    // settings->common.DiscID[5];
    // settings->common.padding[2];
    settings->common.fakemodule_flags = 0;
    settings->common.fakemodule_flags |= FAKE_MODULE_FLAG_CDVDFSV;
    settings->common.fakemodule_flags |= FAKE_MODULE_FLAG_CDVDSTM;

    //
    // Add ISO as fragfile[0] to fragment list
    //
    struct cdvdman_fragfile *iso_frag = &settings->fragfile[0];
    iso_frag->frag_start = 0;
    iso_frag->frag_count = fileXioIoctl2(fd, USBMASS_IOCTL_GET_FRAGLIST, NULL, 0, (void *)&settings->frags[0], sizeof(bd_fragment_t) * BDM_MAX_FRAGS);
    if (iso_frag->frag_count > BDM_MAX_FRAGS) {
        printf("Too many fragments (%d)\n", iso_frag->frag_count);
        return -1;
    }
    close(fd);

    //
    // Patch IOPRP.img with our own CDVDMAN, CDVDFSV and EESYNC
    //
    //printf("IOPRP.img (old):\n");
    //print_romdir(ioprp_img_base.romdir);
    size = patch_IOPRP_image((struct romdir_entry *)irxptr, ioprp_img_base.romdir);
    //printf("IOPRP.img (new):\n");
    //print_romdir((struct romdir_entry *)irxptr);
    irxptr_tab->info = size | SET_OPL_MOD_ID(OPL_MODULE_ID_IOPRP);
    irxptr_tab->ptr = irxptr;
    irxptr_tab++;
    irxptr += size;
    irxtable->count++;

    //
    // Load other modules into place
    //
    irxptr += load_file_mod("imgdrv.irx", irxptr, irxptr_tab++);
    irxtable->count++;
    irxptr += load_file_mod("resetspu.irx", irxptr, irxptr_tab++);
    irxtable->count++;

    // For debugging (udptty) and also udpbd
    //settings->common.fakemodule_flags |= FAKE_MODULE_FLAG_DEV9;
    //irxptr += load_file_mod("ps2dev9.irx", irxptr, irxptr_tab++);
    //irxtable->count++;
    //settings->common.fakemodule_flags |= FAKE_MODULE_FLAG_SMAP;
    //irxptr += load_file_mod("smap.irx", irxptr, irxptr_tab++);
    //irxtable->count++;

    switch (iMode) {
        case BDM_ATA_MODE:
            settings->common.fakemodule_flags |= FAKE_MODULE_FLAG_DEV9;
            irxptr += load_file_mod("ps2dev9.irx", irxptr, irxptr_tab++);
            irxtable->count++;
            settings->common.fakemodule_flags |= FAKE_MODULE_FLAG_ATAD;
            irxptr += load_file_mod("ata_bd.irx", irxptr, irxptr_tab++);
            irxtable->count++;
            break;
        case BDM_USB_MODE:
            settings->common.fakemodule_flags |= FAKE_MODULE_FLAG_USBD;
            irxptr += load_file_mod("usbd_mini.irx", irxptr, irxptr_tab++);
            irxtable->count++;
            irxptr += load_file_mod("usbmass_bd_mini.irx", irxptr, irxptr_tab++);
            irxtable->count++;
            break;
        case BDM_UDP_MODE:
            settings->common.fakemodule_flags |= FAKE_MODULE_FLAG_DEV9;
            irxptr += load_file_mod("ps2dev9.irx", irxptr, irxptr_tab++);
            irxtable->count++;
            settings->common.fakemodule_flags |= FAKE_MODULE_FLAG_SMAP;
            irxptr += load_file_mod("smap.irx", irxptr, irxptr_tab++);
            irxtable->count++;
            break;
        case BDM_M4S_MODE:
            irxptr += load_file_mod("mx4sio_bd_mini.irx", irxptr, irxptr_tab++);
            irxtable->count++;
            break;
        case BDM_ILK_MODE:
            irxptr += load_file_mod("iLinkman.irx", irxptr, irxptr_tab++);
            irxtable->count++;
            irxptr += load_file_mod("IEEE1394_bd_mini.irx", irxptr, irxptr_tab++);
            irxtable->count++;
            break;
    }

    //
    // Load EECORE ELF sections
    //
    u8 *boot_elf = (u8 *)ee_core;
    elf_header_t *eh = (elf_header_t *)boot_elf;
    elf_pheader_t *eph = (elf_pheader_t *)(boot_elf + eh->phoff);
    for (i = 0; i < eh->phnum; i++) {
        if (eph[i].type != ELF_PT_LOAD)
            continue;

        void *pdata = (void *)(boot_elf + eph[i].offset);
        memcpy(eph[i].vaddr, pdata, eph[i].filesz);

        if (eph[i].memsz > eph[i].filesz)
            memset((u8 *)eph[i].vaddr + eph[i].filesz, 0, eph[i].memsz - eph[i].filesz);
    }
    free(ee_core);

    //
    // Patch PS2 to:
    // - use our "ee_core.elf" instead of EELOAD
    // - not wipe our loaded data after reboot
    //
    eeloadCopy = sbvpp_replace_eeload((void *)eh->entry);
    initUserMemory = sbvpp_patch_user_mem_clear(irxptr);

    //
    // Set arguments, and start EECORE
    //
    eecc_init(&eeconf);
    eecc_setGameMode(&eeconf, (enum GAME_MODE)iMode);
    eecc_setKernelConfig(&eeconf, (u32)eeloadCopy, (u32)initUserMemory);
    eecc_setModStorageConfig(&eeconf, (u32)irxtable, (u32)irxptr);
    eecc_setFileName(&eeconf, sConfigName);
    eecc_setDebugColors(&eeconf, true);
    printf("Starting ee_core with following arguments:\n");
    eecc_print(&eeconf);

    ExecPS2((void *)eh->entry, NULL, eecc_argc(&eeconf), (char **)eecc_argv(&eeconf));

    return 0;
}
