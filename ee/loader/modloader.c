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
    printf("ps2client -h 192.168.1.10 execee host:modloader.elf <driver> <path>\n");
    printf("Supported drivers:\n");
    printf(" - usb\n");
    printf("\n");
    printf("Usage example:\n");
    printf("  ps2client -h 192.168.1.10 execee host:modloader.elf usb mass:path/to/filename.iso\n");
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
#define SMF_IOPRP    (1 << 2)
#define SMF_SYSTEM   (1 << 3)
#define SMF_D_USB    (1 << 10)
#define SMF_D_UDPBD  (1 << 11)
#define SMF_D_MX4SIO (1 << 12)
// clang-format off
struct SModule mod[] = {
    {"",        "udnl.irx"           , NULL, 0, false, 0            , OPL_MODULE_ID_UDNL},
    {"",        "IOPRP.img"          , NULL, 0, false, 0            , OPL_MODULE_ID_IOPRP},
    {"CDVDMAN", "bdm_cdvdman.irx"    , NULL, 0, false, SMF_IOPRP    , 0},
    {"CDVDFSV", "cdvdfsv.irx"        , NULL, 0, false, SMF_IOPRP    , 0},
    {"EESYNC",  "eesync.irx"         , NULL, 0, false, SMF_IOPRP    , 0},
    {"",        "imgdrv.irx"         , NULL, 0, false, 0            , OPL_MODULE_ID_IMGDRV},
    {"",        "resetspu.irx"       , NULL, 0, false, 0            , OPL_MODULE_ID_RESETSPU},
    {"",        "iomanX.irx"         , NULL, 0, false, SMF_SYSTEM   , 0},
    {"",        "fileXio.irx"        , NULL, 0, false, SMF_SYSTEM   , 0},
    {"",        "isofs.irx"          , NULL, 0, false, SMF_SYSTEM   , 0},
    {"",        "bdm.irx"            , NULL, 0, false, SMF_SYSTEM   , 0},
    {"",        "bdmfs_vfat.irx"     , NULL, 0, false, SMF_SYSTEM   , 0},
    {"",        "usbd.irx"           , NULL, 0, false, SMF_D_USB    , 0},
    {"",        "mx4sio_bd.irx"      , NULL, 0, false, SMF_D_MX4SIO , 0},
    {"",        "usbmass_bd.irx"     , NULL, 0, false, SMF_D_USB    , 0},
    {"",        "ps2dev9.irx"        , NULL, 0, false, SMF_D_UDPBD  , 0},
    {"",        "smap.irx"           , NULL, 0, false, SMF_D_UDPBD  , 0},
    {NULL, NULL, 0, 0}
};
// clang-format on
struct SModule *mod_cdvdman = &mod[2];

#define MAX_FILENAME 128
int load_module(struct SModule *mod)
{
    char sFilePath[MAX_FILENAME];

    printf("%s(%s)\n", __FUNCTION__, mod->sFileName);

    if (mod->bLoaded == true) {
        printf("WARNING: Module already loaded: %s\n", mod->sFileName);
        return 0;
    }

    // Open module on default location
    snprintf(sFilePath, MAX_FILENAME, "modules/%s", mod->sFileName);
    int fd = open(sFilePath, O_RDONLY);
    if (fd < 0) {
        // Open module on alternative location
        snprintf(sFilePath, MAX_FILENAME, "irx/gt4/%s", mod->sFileName);
        int fd = open(sFilePath, O_RDONLY);
        if (fd < 0) {
            printf("ERROR: Unable to open %s\n", mod->sFileName);
            return -1;
        }
    }

    printf("%s(%s) loaded %s\n", __FUNCTION__, mod->sFileName, sFilePath);

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

    printf("SYSTEM IRX %u address start: %p end: %p\n", mod->eecid, addr, addr + mod->iSize);

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

void load_modules(u32 flags)
{
    int modid;

    for (modid = 0; mod[modid].sFileName != NULL; modid++) {
        if ((mod[modid].iFlags & flags) == flags) {
            load_module(&mod[modid]);
        }
    }
}

void start_modules(u32 flags)
{
    int modid;

    for (modid = 0; mod[modid].sFileName != NULL; modid++) {
        if ((mod[modid].iFlags & flags) == flags) {
            start_module(&mod[modid]);
        }
    }
}

/*----------------------------------------------------------------------------------------
    Replace modules in IOPRP image:
    - CDVDMAN
    - CDVDFSV
    - EESYNC
------------------------------------------------------------------------------------------*/
static unsigned int patch_IOPRP_image(struct romdir_entry *romdir_out, struct romdir_entry *romdir_in)
{
    struct romdir_entry *romdir_out_org = romdir_out;
    u8 *ioprp_in = (u8 *)romdir_in;
    u8 *ioprp_out = (u8 *)romdir_out;

    while (romdir_in->fileName[0] != '\0') {
        struct SModule *mod = load_module_udnlname(romdir_in->fileName);
        if (mod != NULL) {
            printf("IOPRP: replacing %s with %s\n", romdir_in->fileName, mod->sFileName);
            memcpy(ioprp_out, mod->pData, mod->iSize);
            romdir_out->fileSize = mod->iSize;
        } else {
            printf("IOPRP: keeping %s\n", romdir_in->fileName);
            memcpy(ioprp_out, ioprp_in, romdir_in->fileSize);
            romdir_out->fileSize = romdir_in->fileSize;
        }

        // Align all addresses to a multiple of 16
        ioprp_in += (romdir_in->fileSize + 0xF) & ~0xF;
        ioprp_out += (romdir_out->fileSize + 0xF) & ~0xF;
        romdir_in++;
        romdir_out++;
    }

    return (ioprp_out - (u8 *)romdir_out_org);
}

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
    const char *sFileName;
    const char *sDriver;
    char *sConfigName;
    int iLBA;
    int iMode;

    printf("Modular PS2 Game Loader\n");
    printf("  By Maximus32\n");

    // printf("argc = %d\n", argc);
    // for (int i=0; i<argc; i++)
    //     printf("argv[%d] = %s\n", i, argv[i]);

    if (argc != 3) {
        printf("ERROR: argc = %d, should be 3\n", argc);
        print_usage();
        return -1;
    }
    sDriver = argv[1];
    sFileName = argv[2];

    /*
     * Load system drivers
     */
    start_modules(SMF_SYSTEM);

    /*
     * Load file system drivers
     */
    if (!strncmp(sDriver, "usb", 3)) {
        printf("Loading USB drivers\n");
        iMode = BDM_USB_MODE;
    } else if (!strncmp(sDriver, "udpbd", 5)) {
        printf("Loading UDPBD drivers\n");
        iMode = BDM_UDP_MODE;
    } else if (!strncmp(sDriver, "mx4sio", 5)) {
        printf("Loading UDPBD drivers\n");
        iMode = BDM_M4S_MODE;
    } else {
        printf("ERROR: driver %s not supported\n", sDriver);
        print_usage();
        return -1;
    }

    switch (iMode) {
        case BDM_USB_MODE:
            start_modules(SMF_D_USB);
            break;
        case BDM_UDP_MODE:
            start_modules(SMF_D_UDPBD);
            break;
        case BDM_M4S_MODE:
            start_modules(SMF_D_MX4SIO);
            break;
    }

#if 0
    /*
     * Check if file exists
     * Give low level drivers 10s to start
     */
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
    size = lseek64(fd, 0, SEEK_END);
    lseek64(fd, 0, SEEK_SET);
    iLBA = fileXioIoctl(fd, USBMASS_IOCTL_GET_LBA, "");
    int iFrag = fileXioIoctl(fd, USBMASS_IOCTL_CHECK_CHAIN, "");
    printf("Loading %s...\n", sFileName);
    printf("- size = %lldMiB\n", size / (1024 * 1024));
    printf("- LBA  = %d\n", iLBA);
    printf("- frag = %d\n", iFrag);

    if (iFrag != 1) {
        printf("File is fragmented!\n");
        return -1;
    }

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
#else
    //
    // 32GB uSD
    //

    // SCUS_971.13.ICO.iso
    // 30-5: MLR: 260KiB free IOP RAM lowest value (ingame)
    // 30-5: OPL: CRASH!
    //sConfigName = "SCUS_971.13";
    //iLBA = 32768;
    
    // SCUS_973.28.Gran Turismo 4.iso
    // 30-5: MLR: 526KiB free IOP RAM lowest value
    // 30-5: OPL: 518KiB free IOP RAM lowest value
    //sConfigName = "SCUS_973.28";
    //iLBA = 1118896;
    
    // SLES_501.26.Quake III Revolution.iso
    // 30-5: MLR: CRASH!
    // 30-5: OPL: 20KiB free IOP RAM lowest value (ingame)
    //sConfigName = "SLES_501.26";
    //iLBA = 8816816;
    
    // SLES_539.74.Dragon Quest 8.iso
    // 30-5: MLR: 1084KiB free IOP RAM lowest value (ingame) +117KiB !
    // 30-5: OPL:  967KiB free IOP RAM lowest value (ingame)
    //sConfigName = "SLES_539.74";
    //iLBA = 20229424;
    
    // SLES_549.45.DragonBall Z Budokai Tenkaichi 3.iso
    // 30-5: MLR: 239KiB free IOP RAM lowest value (ingame) +117KiB !
    // 30-5: OPL: 122KiB free IOP RAM lowest value (ingame)
    sConfigName = "SLES_549.45";
    iLBA = 10089840;

    //
    // 128GB USB
    //

    // SCES_516.07.Ratchet Clank - Going Commando.iso
    // 30-5: MLR:   5KiB free IOP RAM lowest value (direct daarna 226KiB) +116KiB !
    // 30-5: OPL:   5KiB free IOP RAM lowest value (direct daarna 110KiB)
    //sConfigName = "SCES_516.07";
    //iLBA = 19059392;
    
    // SCES_550.19.Ratchet Clank - Size Matters.iso
    // 30-5: MLR: 186KiB free IOP RAM lowest value +119KiB !
    // 30-5: OPL:  67KiB free IOP RAM lowest value
    //sConfigName = "SCES_550.19";
    //iLBA = 15186368;

    //
    // Direct ISO
    //
    
    // SCUS_974.81.God of War II.iso
    //sConfigName = "SCUS_974.81";
    //iLBA = 0; // ISO as BD
#endif

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
    // Load IOPRP.img
    //
    fd = open("modules/IOPRP.img", O_RDONLY);
    if (fd < 0) {
        printf("Unable to open %s\n", "modules/IOPRP.img");
        return -1;
    }
    size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    void *pIOPRP = malloc(size);
    read(fd, pIOPRP, size);
    close(fd);

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
    settings->common.flags = IOPCORE_COMPAT_ACCU_READS;
    // settings->common.layer1_start = 0x0;
    // settings->common.DiscID[5];
    // settings->common.padding[3];
    settings->LBAs[0] = iLBA;
    // settings->LBAs[1];

    //
    // Patch IOPRP.img with our own CDVDMAN, CDVDFSV and EESYNC
    //
    printf("IOPRP.img (old):\n");
    print_romdir(pIOPRP);
    size = patch_IOPRP_image((struct romdir_entry *)irxptr, (struct romdir_entry *)pIOPRP);
    printf("IOPRP.img (new):\n");
    print_romdir((void *)irxptr);
    irxptr_tab->info = size | SET_OPL_MOD_ID(OPL_MODULE_ID_IOPRP);
    irxptr_tab->ptr = irxptr;
    irxptr_tab++;
    free(pIOPRP);
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
    //irxptr += load_file_mod("ps2dev9.irx", irxptr, irxptr_tab++);
    //irxtable->count++;
    irxptr += load_file_mod("smap.irx", irxptr, irxptr_tab++);
    irxtable->count++;

    switch (iMode) {
        case BDM_USB_MODE:
            irxptr += load_file_mod("usbd.irx", irxptr, irxptr_tab++);
            irxtable->count++;
            irxptr += load_file_mod("usbmass_bd.irx", irxptr, irxptr_tab++);
            irxtable->count++;
            break;
        case BDM_UDP_MODE:
            //irxptr += load_file_mod("smap.irx", irxptr, irxptr_tab++);
            //irxtable->count++;
            break;
        case BDM_M4S_MODE:
            irxptr += load_file_mod("mx4sio_bd.irx", irxptr, irxptr_tab++);
            irxtable->count++;
            break;
    }

    //
    // Load EECORE file
    //
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
    eecc_setGameMode(&eeconf, iMode);
    eecc_setKernelConfig(&eeconf, (u32)eeloadCopy, (u32)initUserMemory);
    eecc_setModStorageConfig(&eeconf, (u32)irxtable, (u32)irxptr);
    eecc_setFileName(&eeconf, sConfigName);
    eecc_setDebugColors(&eeconf, true);
    printf("Starting ee_core with following arguments:\n");
    eecc_print(&eeconf);

    ExecPS2((void *)eh->entry, NULL, eecc_argc(&eeconf), (char **)eecc_argv(&eeconf));

    return 0;
}
