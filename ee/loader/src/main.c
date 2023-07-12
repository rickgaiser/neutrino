// libc/newlib
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
_off64_t lseek64 (int __filedes, _off64_t __offset, int __whence); // should be defined in unistd.h ???

// PS2SDK
#include <kernel.h>
#include <ps2sdkapi.h>
#include <tamtypes.h>
#include <loadfile.h>
#include <iopcontrol.h>
#include <usbhdfsd-common.h>
#include <libcdvd-common.h>

// Other
#include "elf.h"
#include "compat.h"
#include "patch.h"
#include "modules.h"
#include "ee_core_config.h"
#include "ioprp.h"
#include "xparam.h"
#include "../../../iop/common/cdvd_config.h"

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <io_common.h>

DISABLE_PATCHED_FUNCTIONS();      // Disable the patched functionalities
DISABLE_EXTRA_TIMERS_FUNCTIONS(); // Disable the extra functionalities for timers
PS2_DISABLE_AUTOSTART_PTHREAD();  // Disable pthread functionality

#ifdef BUILTIN_COMMON
    #define IRX_COMMON_DEFINE(mod)        \
        extern unsigned char mod##_irx[]; \
        extern unsigned int size_##mod##_irx
    #define ELF_DEFINE(mod)               \
        extern unsigned char mod##_elf[]; \
        extern unsigned int size_##mod##_elf
#else
    #define IRX_COMMON_DEFINE(mod)             \
        const unsigned char *mod##_irx = NULL; \
        const unsigned int size_##mod##_irx = 0
    #define ELF_DEFINE(mod)               \
        const unsigned char *mod##_elf = NULL; \
        const unsigned int size_##mod##_elf = 0
#endif

#ifdef BUILTIN_DRIVERS
    #define IRX_DRIVER_DEFINE(mod)        \
        extern unsigned char mod##_irx[]; \
        extern unsigned int size_##mod##_irx
#else
    #define IRX_DRIVER_DEFINE(mod)        \
        const unsigned char *mod##_irx = NULL; \
        const unsigned int size_##mod##_irx = 0
#endif

IRX_COMMON_DEFINE(cdvdfsv);
IRX_COMMON_DEFINE(bdm_cdvdman);
IRX_COMMON_DEFINE(imgdrv);
IRX_COMMON_DEFINE(isofs);
IRX_COMMON_DEFINE(resetspu);
IRX_COMMON_DEFINE(eesync);
IRX_COMMON_DEFINE(udnl);
IRX_COMMON_DEFINE(iomanX);
IRX_COMMON_DEFINE(fileXio);
IRX_COMMON_DEFINE(bdm);
IRX_COMMON_DEFINE(bdmfs_fatfs);

IRX_DRIVER_DEFINE(smap);
IRX_DRIVER_DEFINE(ata_bd);
IRX_DRIVER_DEFINE(usbd_mini);
IRX_DRIVER_DEFINE(usbmass_bd_mini);
IRX_DRIVER_DEFINE(mx4sio_bd_mini);
IRX_DRIVER_DEFINE(ps2dev9);
IRX_DRIVER_DEFINE(iLinkman);
IRX_DRIVER_DEFINE(IEEE1394_bd_mini);

ELF_DEFINE(ee_core);

#define IP_ADDR(a, b, c, d) ((a << 24) | (b << 16) | (c << 8) | d)

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
    printf("Usage: neutrino.elf -drv=<driver> -iso=<path>\n");
    printf("\n");
    printf("Options:\n");
    printf("  -drv=<driver>     Select block device driver, supported are: ata, usb, mx4sio(sdc), udpbd(udp) and ilink(sd)\n");
    printf("  -iso=<file>       Select iso file (full path!)\n");
    printf("  -mt=<type>        Select media type, supported are: cd, dvd. Defaults to cd for size<=650MiB, and dvd for size>650MiB\n");
    printf("  -gc=<compat>      Game compatibility modes, supperted are:\n");
    printf("                    - 0: Disable builtin compat flags\n");
    printf("                    - 1: IOP: Accurate reads (sceCdRead)\n");
    printf("                    - 2: IOP: Sync reads (sceCdRead)\n");
    printf("                    - 3: EE : Unhook syscalls\n");
    printf("                    - 5: IOP: Emulate DVD-DL\n");
    printf("                    Multiple options possible, for example -cp=26\n");
    printf("  -ip=<ip>          Set IP adres for udpbd, default: 192.168.1.10\n");
    printf("  -nR               No reboot before loading the iso (faster)\n");
    printf("  -eC               Enable eecore debug colors\n");
    printf("\n");
    printf("Usage example:\n");
    printf("  ps2client -h 192.168.1.10 execee host:neutrino.elf -drv=usb -iso=mass:path/to/filename.iso\n");
}

struct SModule
{
    const char *sUDNL;
    const char *sFileName;
    void *pData;
    off_t iSize;
    u32 iFlags;
    u32 eecid;
};
#define SMF_IOPCORE  (1 << 2)
#define SMF_FIOX     (1 << 3)
#define SMF_ISO      (1 << 4)
#define SMF_BDMFS    (1 << 5)
#define SMF_EECORE   (1 << 6)
#define SMF_D_ATA    (1 << 10)
#define SMF_D_USB    (1 << 11)
#define SMF_D_UDPBD  (1 << 12)
#define SMF_D_MX4SIO (1 << 13)
#define SMF_D_ILINK  (1 << 14)
// clang-format off
struct SModule mod[] = {
    {"",        "udnl.irx"             , NULL, 0, SMF_IOPCORE  , OPL_MODULE_ID_UDNL},
    {"CDVDMAN", "bdm_cdvdman.irx"      , NULL, 0, SMF_IOPCORE  , 0},
    {"CDVDFSV", "cdvdfsv.irx"          , NULL, 0, SMF_IOPCORE  , 0},
    {"EESYNC",  "eesync.irx"           , NULL, 0, SMF_IOPCORE  , 0},
    {"",        "imgdrv.irx"           , NULL, 0, SMF_IOPCORE  , OPL_MODULE_ID_IMGDRV},
    {"",        "resetspu.irx"         , NULL, 0, SMF_IOPCORE  , 0},
    {"",        "iomanX.irx"           , NULL, 0, SMF_FIOX     , 0},
    {"",        "fileXio.irx"          , NULL, 0, SMF_FIOX     , 0},
    {"",        "isofs.irx"            , NULL, 0, SMF_ISO      , 0},
    {"",        "bdm.irx"              , NULL, 0, SMF_BDMFS    , 0},
    {"",        "bdmfs_fatfs.irx"      , NULL, 0, SMF_BDMFS    , 0},
    {"",        "usbd_mini.irx"        , NULL, 0, SMF_D_USB    , OPL_MODULE_ID_USBD},
    {"",        "usbmass_bd_mini.irx"  , NULL, 0, SMF_D_USB    , 0},
    {"",        "mx4sio_bd_mini.irx"   , NULL, 0, SMF_D_MX4SIO , 0},
    {"",        "ps2dev9.irx"          , NULL, 0, SMF_D_ATA|SMF_D_UDPBD, 0},
    {"",        "ata_bd.irx"           , NULL, 0, SMF_D_ATA    , 0},
    {"",        "smap.irx"             , NULL, 0, SMF_D_UDPBD  , 0},
    {"",        "iLinkman.irx"         , NULL, 0, SMF_D_ILINK  , 0},
    {"",        "IEEE1394_bd_mini.irx" , NULL, 0, SMF_D_ILINK  , 0},
    {"",        "ee_core.elf"          , NULL, 0, SMF_EECORE   , 0},
    {NULL, NULL, 0, 0}
};
// clang-format on

#define INIT_MOD(nr,name) \
    mod[nr].pData = (void *)name##_irx; \
    mod[nr].iSize = size_##name##_irx

#define INIT_ELF(nr,name) \
    mod[nr].pData = (void *)name##_elf; \
    mod[nr].iSize = size_##name##_elf

void mod_init()
{
    INIT_MOD( 0, udnl);
    INIT_MOD( 1, bdm_cdvdman);
    INIT_MOD( 2, cdvdfsv);
    INIT_MOD( 3, eesync);
    INIT_MOD( 4, imgdrv);
    INIT_MOD( 5, resetspu);
    INIT_MOD( 6, iomanX);
    INIT_MOD( 7, fileXio);
    INIT_MOD( 8, isofs);
    INIT_MOD( 9, bdm);
    INIT_MOD(10, bdmfs_fatfs);
    INIT_MOD(11, usbd_mini);
    INIT_MOD(12, usbmass_bd_mini);
    INIT_MOD(13, mx4sio_bd_mini);
    INIT_MOD(14, ps2dev9);
    INIT_MOD(15, ata_bd);
    INIT_MOD(16, smap);
    INIT_MOD(17, iLinkman);
    INIT_MOD(18, IEEE1394_bd_mini);
    INIT_ELF(19, ee_core);
}

#define MAX_FILENAME 128
int load_module(struct SModule *mod)
{
    char sFilePath[MAX_FILENAME];

    //printf("%s(%s)\n", __FUNCTION__, mod->sFileName);

    if (mod->pData != NULL) {
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

    return 0;
}

struct SModule *get_module_name(const char *name)
{
    int modid;

    for (modid = 0; mod[modid].sFileName != NULL; modid++) {
        if (strcmp(mod[modid].sFileName, name) == 0) {
            return &mod[modid];
        }
    }

    return NULL;
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
    int rv;

    if (mod->pData == NULL) {
        int rv = load_module(mod);
        if (rv < 0)
            return rv;
    }

    rv = SifExecModuleBuffer(mod->pData, mod->iSize, 0, NULL, NULL);
    if (rv < 0) {
        printf("ERROR: Could not load %s (%d)\n", mod->sFileName, rv);
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

u32 parse_ip(const char *sIP)
{
    int cp = 0;
    u32 part[4] = {0,0,0,0};

    while(*sIP != 0) {
        //printf("%s\n", sIP);
        if(*sIP == '.') {
            cp++;
            if (cp >= 4)
                return 0; // Too many dots
        } else if(*sIP >= '0' && *sIP <= '9') {
            part[cp] = (part[cp] * 10) + (*sIP - '0');
            if (part[cp] > 255)
                return 0; // Too big number
        } else {
            return 0; // Invalid character
        }
        sIP++;
    }

    if (cp != 3)
        return 0; // Too little dots

    return IP_ADDR(part[0], part[1], part[2], part[3]);
}


int main(int argc, char *argv[])
{
    irxtab_t *irxtable;
    irxptr_t *irxptr_tab;
    u8 *irxptr;
    int fd;
    int i;
    void *eeloadCopy, *initUserMemory;
    const char *sGameID;
    int iMode;
    int iDrivers;

    mod_init();

    printf("----------------------------\n");
    printf("- Neutrino PS2 Game Loader -\n");
    printf("-       By Maximus32       -\n");
    printf("----------------------------\n");

    const char *sDriver = NULL;
    const char *sFileName = NULL;
    const char *sIP = NULL;
    const char *sMediaType = NULL;
    const char *sCompat = NULL;
    u32 iIP = 0;
    u32 iCompat = 0;
    enum SCECdvdMediaType eMediaType = SCECdNODISC;
    int iNoReboot = 0;
    int iEnableDebugColors = 0;
    for (i=1; i<argc; i++) {
        //printf("argv[%d] = %s\n", i, argv[i]);
        if (!strncmp(argv[i], "-drv=", 5))
            sDriver = &argv[i][5];
        else if (!strncmp(argv[i], "-iso=", 5))
            sFileName = &argv[i][5];
        else if (!strncmp(argv[i], "-ip=", 4))
            sIP = &argv[i][4];
        else if (!strncmp(argv[i], "-mt=", 4))
            sMediaType = &argv[i][4];
        else if (!strncmp(argv[i], "-gc", 3))
            sCompat = &argv[i][4];
        else if (!strncmp(argv[i], "-nR", 3))
            iNoReboot = 1;
        else if (!strncmp(argv[i], "-eC", 3))
            iEnableDebugColors = 1;
        else {
            printf("ERROR: unknown argv[%d] = %s\n", i, argv[i]);
            print_usage();
            return -1;
        }
    }

    if (sMediaType != NULL) {
        if (!strncmp(sMediaType, "cdda", 4)) {
            eMediaType = SCECdPS2CD;
        } else if (!strncmp(sMediaType, "cd", 2)) {
            eMediaType = SCECdPS2CDDA;
        } else if (!strncmp(sMediaType, "dvd", 3)) {
            eMediaType = SCECdPS2DVD;
        } else {
            printf("ERROR: media type %s not supported\n", sMediaType);
            print_usage();
            return -1;
        }
    }

    if (sCompat != NULL) {
        while (*sCompat != 0) {
            char c = *sCompat;
            switch (c) {
                case '0':
                    iCompat |= 1 << 31; // Set dummy flag
                    break;
                case '1':
                case '2':
                case '3':
                case '5':
                    iCompat |= 1 << (c - '1');
                    break;
                default:
                    printf("ERROR: compat flag %c not supported\n", c);
                    print_usage();
                    return -1;
            }
            sCompat++;
        }
    }

    if (sIP != NULL) {
        iIP = parse_ip(sIP);
        if (iIP == 0) {
            printf("ERROR: cannot parse IP\n");
            print_usage();
            return -1;
        }
    }

    /*
     * Figure out what drivers we need
     */
    if (!strncmp(sDriver, "ata", 3)) {
        printf("Loading ATA drivers\n");
        iMode = BDM_ATA_MODE;
        iDrivers = SMF_D_ATA;
    } else if (!strncmp(sDriver, "usb", 3)) {
        printf("Loading USB drivers\n");
        iMode = BDM_USB_MODE;
        iDrivers = SMF_D_USB;
    } else if (!strncmp(sDriver, "udpbd", 5) || !strncmp(sDriver, "udp", 3)) {
        printf("Loading UDPBD drivers\n");
        iMode = BDM_UDP_MODE;
        iDrivers = SMF_D_UDPBD;
    } else if (!strncmp(sDriver, "mx4sio", 6) || !strncmp(sDriver, "sdc", 3)) {
        printf("Loading MX4SIO drivers\n");
        iMode = BDM_M4S_MODE;
        iDrivers = SMF_D_MX4SIO;
    } else if (!strncmp(sDriver, "ilink", 5) || !strncmp(sDriver, "sd", 2)) {
        printf("Loading iLink drivers\n");
        iMode = BDM_ILK_MODE;
        iDrivers = SMF_D_ILINK;
    } else {
        printf("ERROR: driver %s not supported\n", sDriver);
        print_usage();
        return -1;
    }
    iDrivers |= SMF_BDMFS;
    iDrivers |= SMF_FIOX;
    iDrivers |= SMF_ISO;
#ifdef DEBUG
    iDrivers |= SMF_D_UDPBD;
#endif

    /*
     * Load drivers before rebooting the IOP
     */
    if (load_modules(iDrivers|SMF_EECORE|SMF_IOPCORE) < 0)
        return -1;

    if (iNoReboot == 0) {
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
    }

    /*
     * Start system drivers
     */
    if (start_modules(iDrivers))
        return -1;
    fileXioInit();

    /*
     * Check if file exists
     * Give low level drivers 10s to start
     */
    printf("Loading %s...\n", sFileName);
    for (i = 0; i < 10000; i++) {
        fd = open(sFileName, O_RDONLY);
        if (fd >= 0)
            break;

        // Give low level drivers some time to init
        nopdelay();
    }
    if (fd < 0) {
        printf("Unable to open %s\n", sFileName);
        return -1;
    }
    // Get ISO file size
    off_t iso_size = lseek64(fd, 0, SEEK_END);
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
    printf("- size = %dMiB\n", (int)(iso_size / (1024 * 1024)));

    if (eMediaType == SCECdNODISC)
        eMediaType = iso_size <= (333000 * 2048) ? SCECdPS2CD : SCECdPS2DVD;
    printf("- media = %s\n", eMediaType == SCECdPS2DVD ? "DVD" : "CD");

    /*
     * Mount as ISO so we can get some information
     */
    int fd_isomount = fileXioMount("iso:", sFileName, FIO_MT_RDONLY);
    if (fd_isomount < 0) {
        printf("ERROR: Unable to mount %s as iso\n", sFileName);
        return -1;
    }
    int fd_config = open("iso:\\SYSTEM.CNF;1", O_RDONLY);
    if (fd_config < 0) {
        printf("ERROR: Unable to open %s\n", "iso:\\SYSTEM.CNF;1");
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
    sGameID = &fname_start[8];
    fname_end[0] = '\0';
    printf("config name: %s\n", sGameID);
    close(fd_config);
    fileXioUmount("iso:");

    ResetDeckardXParams();
    ApplyDeckardXParam(sGameID);

    //
    // Locate and set cdvdman settings
    //
    struct SModule *mod_cdvdman = get_module_name("bdm_cdvdman.irx");
    struct cdvdman_settings_bdm *settings = NULL;
    for (i = 0; i < mod_cdvdman->iSize; i += 4) {
        if (!memcmp((void *)((u8 *)mod_cdvdman->pData + i), &cdvdman_settings_common_sample, sizeof(cdvdman_settings_common_sample))) {
            settings = (struct cdvdman_settings_bdm *)((u8 *)mod_cdvdman->pData + i);
            break;
        }
    }
    if (settings == NULL) {
        printf("ERROR: unable to locate cdvdman settings\n");
        return -1;
    }
    memset((void *)settings, 0, sizeof(struct cdvdman_settings_bdm));
    settings->common.media = eMediaType;
    settings->common.layer1_start = layer1_lba_start;
    // settings->common.DiscID[5];
    // settings->common.padding[2];
    settings->common.fakemodule_flags = 0;
    settings->common.fakemodule_flags |= FAKE_MODULE_FLAG_CDVDFSV;
    settings->common.fakemodule_flags |= FAKE_MODULE_FLAG_CDVDSTM;

    // If no compatibility options are set on the command line
    // see if the game is in our builtin database
    if (iCompat == 0)
        iCompat = get_compat(sGameID);
    iCompat &= ~(1<<31); // Clear dummy flag
    if (iCompat & COMPAT_MODE_1)
        settings->common.flags |= IOPCORE_COMPAT_ACCU_READS;
    if (iCompat & COMPAT_MODE_2)
        settings->common.flags |= IOPCORE_COMPAT_ALT_READ;
    if (iCompat & COMPAT_MODE_5)
        settings->common.flags |= IOPCORE_COMPAT_EMU_DVDDL;
    printf("Compat flags: 0x%X, IOP=0x%X\n", iCompat, settings->common.flags);

    //
    // Add ISO as fragfile[0] to fragment list
    //
    struct cdvdman_fragfile *iso_frag = &settings->fragfile[0];
    iso_frag->frag_start = 0;
    iso_frag->frag_count = fileXioIoctl2(fd, USBMASS_IOCTL_GET_FRAGLIST, NULL, 0, (void *)&settings->frags[iso_frag->frag_start], sizeof(bd_fragment_t) * (BDM_MAX_FRAGS - iso_frag->frag_start));
    iso_frag->size       = iso_size;
    printf("ISO fragments: start=%u, count=%u\n", iso_frag->frag_start, iso_frag->frag_count);
    for (i=0; i<iso_frag->frag_count; i++) {
        printf("- frag[%d] start=%u, count=%u\n", i, (u32)settings->frags[iso_frag->frag_start+i].sector, settings->frags[iso_frag->frag_start+i].count);
    }
    if ((iso_frag->frag_start + iso_frag->frag_count) > BDM_MAX_FRAGS) {
        printf("Too many fragments (%d)\n", iso_frag->frag_start + iso_frag->frag_count);
        return -1;
    }
    close(fd);

    //
    // Locate and set smap settings
    //
    struct SModule *mod_smap = get_module_name("smap.irx");
    if (iIP != 0 && mod_smap->pData != NULL) {
        uint32_t ip_default = IP_ADDR(192, 168, 1, 10);
        uint32_t *ip_smap = NULL;
        for (i = 0; i < mod_smap->iSize; i += 4) {
            if (!memcmp((void *)((u8 *)mod_smap->pData + i), &ip_default, sizeof(ip_default))) {
                ip_smap = (uint32_t *)((u8 *)mod_smap->pData + i);
                break;
            }
        }
        if (ip_smap == NULL) {
            printf("ERROR: unable to locate smap IP\n");
            return -1;
        }
        *ip_smap = iIP;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
    memset((void *)0x00084000, 0, 0x00100000 - 0x00084000);
#pragma GCC diagnostic pop

    irxtable = (irxtab_t *)get_modstorage(sGameID);
    if (irxtable == NULL)
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
    // Patch IOPRP.img with our own CDVDMAN, CDVDFSV and EESYNC
    //
    //printf("IOPRP.img (old):\n");
    //print_romdir(ioprp_img_base.romdir);
    unsigned int ioprp_size = patch_IOPRP_image((struct romdir_entry *)irxptr, ioprp_img_base.romdir);
    //printf("IOPRP.img (new):\n");
    //print_romdir((struct romdir_entry *)irxptr);
    irxptr_tab->info = ioprp_size | SET_OPL_MOD_ID(OPL_MODULE_ID_IOPRP);
    irxptr_tab->ptr = irxptr;
    irxptr_tab++;
    irxptr += ioprp_size;
    irxtable->count++;

    //
    // Load other modules into place
    //
    irxptr += load_file_mod("imgdrv.irx", irxptr, irxptr_tab++);
    irxtable->count++;
    irxptr += load_file_mod("resetspu.irx", irxptr, irxptr_tab++);
    irxtable->count++;

#ifdef DEBUG
    // For debugging (udptty) and also udpbd
    settings->common.fakemodule_flags |= FAKE_MODULE_FLAG_DEV9;
    irxptr += load_file_mod("ps2dev9.irx", irxptr, irxptr_tab++);
    irxtable->count++;
    settings->common.fakemodule_flags |= FAKE_MODULE_FLAG_SMAP;
    irxptr += load_file_mod("smap.irx", irxptr, irxptr_tab++);
    irxtable->count++;
#endif

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
#ifndef DEBUG
            settings->common.fakemodule_flags |= FAKE_MODULE_FLAG_DEV9;
            irxptr += load_file_mod("ps2dev9.irx", irxptr, irxptr_tab++);
            irxtable->count++;
            settings->common.fakemodule_flags |= FAKE_MODULE_FLAG_SMAP;
            irxptr += load_file_mod("smap.irx", irxptr, irxptr_tab++);
            irxtable->count++;
#endif
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
    u8 *boot_elf = (u8 *)get_module_name("ee_core.elf")->pData;
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
    eecc_setFileName(&eeconf, sGameID);
    eecc_setCompatFlags(&eeconf, iCompat);
    eecc_setDebugColors(&eeconf, iEnableDebugColors ? true : false);
    printf("Starting ee_core with following arguments:\n");
    eecc_print(&eeconf);

    ExecPS2((void *)eh->entry, NULL, eecc_argc(&eeconf), (char **)eecc_argv(&eeconf));

    return 0;
}
