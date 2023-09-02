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
#include "../../../iop/common/fakemod.h"
#include "toml.h"

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <io_common.h>

DISABLE_PATCHED_FUNCTIONS();      // Disable the patched functionalities
DISABLE_EXTRA_TIMERS_FUNCTIONS(); // Disable the extra functionalities for timers
PS2_DISABLE_AUTOSTART_PTHREAD();  // Disable pthread functionality

#define OPL_MOD_STORAGE 0x00097000 //(default) Address of the module storage region
/*
OPL Module Storage Memory Map:
    struct irxtab_t;
    struct irxptr_tab[modcount];
    IOPRP.img, containing:
    - cdvdman.irx
    - cdvdfsv.irx
    - eesync.irx
    imgdrv.irx
    <extra modules>
*/

static struct SEECoreConfig eeconf;

void print_usage()
{
    printf("Usage: neutrino.elf -drv=<driver> -iso=<path>\n");
    printf("\n");
    printf("Options:\n");
    printf("  -drv=<driver>     Select block device driver, supported are:\n");
    printf("                    - ata\n");
    printf("                    - usb\n");
    printf("                    - mx4sio\n");
    printf("                    - udpbd\n");
    printf("                    - ilink\n");
    printf("                    - dvd\n");
    printf("                    - esr\n");
    printf("  -iso=<file>       Select iso file (full path!)\n");
    printf("  -elf=<file>       Select elf file inside iso to boot\n");
    printf("  -mt=<type>        Select media type, supported are:\n");
    printf("                    - cd\n");
    printf("                    - dvd\n");
    printf("                    Defaults to cd for size<=650MiB, and dvd for size>650MiB\n");
    printf("  -gc=<compat>      Game compatibility modes, supported are:\n");
    printf("                    - 0: Disable builtin compat flags\n");
    printf("                    - 1: IOP: Accurate reads (sceCdRead)\n");
    printf("                    - 2: IOP: Sync reads (sceCdRead)\n");
    printf("                    - 3: EE : Unhook syscalls\n");
    printf("                    - 5: IOP: Emulate DVD-DL\n");
    printf("                    Multiple options possible, for example -gc=23\n");
    printf("  -eC               Enable eecore debug colors\n");
    printf("\n");
    printf("Usage examples:\n");
    printf("  ps2client -h 192.168.1.10 execee host:neutrino.elf -drv=usb -iso=mass:path/to/filename.iso\n");
    printf("  ps2client -h 192.168.1.10 execee host:neutrino.elf -drv=dvd\n");
    printf("  ps2client -h 192.168.1.10 execee host:neutrino.elf -drv=esr\n");
}

struct SModule
{
    const char *sFileName;
    const char *sUDNL;

    off_t iSize;
    u8 *pData;

    const char *args;
};

#define DRV_MAX_MOD 20
struct SModList {
    int count;
    struct SModule *mod;
};

struct SFakeList {
    int count;
    struct FakeModule *fake;
};

struct SSystemSettings {
    union {
        uint8_t ilink_id[8];
        uint64_t ilink_id_int;
    };

    union {
        uint8_t disk_id[5];
        uint64_t disk_id_int; // 8 bytes, but that's ok for compare reasons
    };
} sys;

struct SDriver {
    const char *name;
    const char *type;
    const char *mode;

    // System drivers for load environment
    struct SModList mod_lsys;
    // System drivers for ingame environment
    struct SModList mod_isys;
    // Device drivers (for both environments)
    struct SModList mod_drv;

    // Module faking for ingame environment
    struct SFakeList fake;
} drv;

struct SModule mod_ee_core = {"ee_core.elf"};

#define MAX_FILENAME 128
int module_load(struct SModule *mod)
{
    char sFilePath[MAX_FILENAME];

    //printf("%s(%s)\n", __FUNCTION__, mod->sFileName);

    if (mod->pData != NULL) {
        printf("WARNING: Module already loaded: %s\n", mod->sFileName);
        return 0;
    }

    if (mod->sFileName == NULL) {
        return -1;
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
    mod->pData = malloc(mod->iSize); // NOTE: never freed, but we don't care

    // Load module into memory
    read(fd, mod->pData, mod->iSize);

    // Close module
    close(fd);

    return 0;
}

int modlist_load(struct SModList *ml)
{
    int i;

    for (i = 0; i < ml->count; i++) {
        if (module_load(&ml->mod[i]) < 0)
            return -1;
    }

    return 0;
}

int module_start(struct SModule *mod)
{
    int rv, IRX_ID;

    if (mod->pData == NULL) {
        printf("ERROR: %s not loaded\n", mod->sFileName);
        return -1;
    }

    IRX_ID = SifExecModuleBuffer(mod->pData, mod->iSize, (mod->args == NULL) ? 0 : strlen(mod->args), mod->args, &rv);
    if (IRX_ID < 0 || rv == 1) {
        printf("ERROR: Could not load %s (ID+%d, rv=%d)\n", mod->sFileName, IRX_ID, rv);
        return -1;
    }

    printf("- %s started\n", mod->sFileName);

    return 0;
}

int modlist_start(struct SModList *ml)
{
    int i;

    for (i = 0; i < ml->count; i++) {
        if (module_start(&ml->mod[i]) < 0)
            return -1;
    }

    return 0;
}

struct SModule *modlist_get_by_name(struct SModList *ml, const char *name)
{
    int i;

    for (i = 0; i < ml->count; i++) {
        if (strcmp(ml->mod[i].sFileName, name) == 0) {
            return &ml->mod[i];
        }
    }

    return NULL;
}

struct SModule *modlist_get_by_udnlname(struct SModList *ml, const char *name)
{
    int i;

    for (i = 0; i < ml->count; i++) {
        struct SModule *m = &ml->mod[i];
        if (m->sUDNL != NULL) {
            if (strcmp(m->sUDNL, name) == 0)
                return m;
        }
    }

    return NULL;
}

static u8 * module_install(struct SModule *mod, u8 *addr, irxptr_t *irx)
{
    if (mod == NULL) {
        printf("ERROR: mod == NULL\n");
        return 0;
    }

    // Install module
    memcpy(addr, mod->pData, mod->iSize);
    irx->size = mod->iSize;
    irx->ptr = addr;
    addr += mod->iSize;

    // Install module arguments
    if (mod->args == NULL) {
        irx->arg_len = 0;
        irx->args = NULL;
    }
    else {
        irx->arg_len = strlen(mod->args) + 1;
        memcpy(addr, mod->args, irx->arg_len);
        irx->args = (char *)addr;
        addr += irx->arg_len;
    }

    printf("Module %s installed to 0x%p\n", mod->sFileName, irx->ptr);
    if (mod->args != NULL)
        printf("- args = \"%s\" installed to 0x%p\n", mod->args, irx->args);

    // Align to 16 bytes
    return (u8 *)((u32)(addr + 0xf) & ~0xf);
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
        struct SModule *mod = modlist_get_by_udnlname(&drv.mod_isys, romdir_in->name);
        if (mod != NULL) {
            printf("IOPRP: replacing %s with %s\n", romdir_in->name, mod->sFileName);
            memcpy(ioprp_out, mod->pData, mod->iSize);
            romdir_out->size = mod->iSize;
        } else {
            printf("IOPRP: keeping %s\n", romdir_in->name);
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

struct ioprp_ext_full {
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
struct ioprp_img_full
{
    romdir_entry_t romdir[7];
    struct ioprp_ext_full ext;
};
static const struct ioprp_img_full ioprp_img_full = {
    {{"RESET"  ,  8, 0},
     {"ROMDIR" ,  0, 0x10 * 7},
     {"EXTINFO",  0, sizeof(struct ioprp_ext_full)},
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

struct ioprp_ext_dvd {
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
struct ioprp_img_dvd
{
    romdir_entry_t romdir[5];
    struct ioprp_ext_dvd ext;
};
static const struct ioprp_img_dvd ioprp_img_dvd = {
    {{"RESET"  ,  8, 0},
     {"ROMDIR" ,  0, 0x10 * 5},
     {"EXTINFO",  0, sizeof(struct ioprp_ext_dvd)},
     {"EESYNC" , 24, 0},
     {"", 0, 0}},
    {
        // RESET extinfo
        {0, 4, EXTINFO_TYPE_DATE},
        0x20230621,
        // SYNCEE extinfo
        {0, 4, EXTINFO_TYPE_DATE},
        0x20230621,
        {0x9999, 0, EXTINFO_TYPE_VERSION},
        {0, 8, EXTINFO_TYPE_COMMENT},
        "SyncEE"
    }};

int modlist_add(struct SModList *ml, toml_table_t *t)
{
    toml_datum_t v;
    struct SModule *m;

    if (ml->count >= DRV_MAX_MOD)
        return -1;
    m = &ml->mod[ml->count];
    ml->count++;

    v = toml_string_in(t, "file");
    if (v.ok)
        m->sFileName = v.u.s; // NOTE: passing ownership of dynamic memory
    v = toml_string_in(t, "ioprp");
    if (v.ok)
        m->sUDNL = v.u.s; // NOTE: passing ownership of dynamic memory
    v = toml_string_in(t, "args");
    if (v.ok)
        m->args = v.u.s; // NOTE: passing ownership of dynamic memory

    return 0;
}

int modlist_add_array(struct SModList *ml, toml_table_t *tbl_root)
{
    int i;
    toml_array_t *arr = toml_array_in(tbl_root, "module");
    if (arr == NULL)
        return 0;

    for (i=0; i < toml_array_nelem(arr); i++) {
        toml_table_t *t = toml_table_at(arr, i);
        if (t == NULL) {
            free(arr);
            return -1;
        }
        if (modlist_add(ml, t) < 0) {
            free(t);
            free(arr);
            return -1;
        }
        free(t);
    }
    free(arr);

    return 0;
}

int fakelist_add(struct SFakeList *fl, toml_table_t *t)
{
    toml_datum_t v;
    struct FakeModule *f;

    if (fl->count >= MODULE_SETTINGS_MAX_FAKE_COUNT)
        return -1;
    f = &fl->fake[fl->count];
    fl->count++;

    v = toml_string_in(t, "file");
    if (v.ok)
        f->fname = v.u.s; // NOTE: passing ownership of dynamic memory
    v = toml_string_in(t, "name");
    if (v.ok)
        f->name = v.u.s; // NOTE: passing ownership of dynamic memory
    v = toml_bool_in(t, "replace");
    if (v.ok)
        f->prop |= (v.u.b != 0) ? FAKE_PROP_REPLACE : 0;
    v = toml_bool_in(t, "unload");
    if (v.ok)
        f->prop |= (v.u.b != 0) ? FAKE_PROP_UNLOAD : 0;
    v = toml_int_in(t, "version");
    if (v.ok)
        f->version = v.u.i;
    v = toml_int_in(t, "return");
    if (v.ok)
        f->returnValue = v.u.i;

    return 0;
}

int fakelist_add_array(struct SFakeList *fl, toml_table_t *tbl_root)
{
    int i;
    toml_array_t *arr = toml_array_in(tbl_root, "fake");
    if (arr == NULL)
        return 0;

    for (i=0; i < toml_array_nelem(arr); i++) {
        toml_table_t *t = toml_table_at(arr, i);
        if (t == NULL) {
            free(arr);
            return -1;
        }
        if (fakelist_add(fl, t) < 0) {
            free(t);
            free(arr);
            return -1;
        }
        free(t);
    }
    free(arr);

    return 0;
}

int load_driver(const char * driver)
{
    FILE* fp;
    char filename[256];
    char errbuf[200];
    toml_table_t *tbl_root = NULL;
    toml_datum_t v;

    // Initialize driver structure
    memset(&drv, 0, sizeof(struct SDriver));
    drv.mod_drv.mod  = malloc(DRV_MAX_MOD * sizeof(struct SModule)); // NOTE: never freed, but we don't care
    memset(drv.mod_drv.mod,  0, DRV_MAX_MOD * sizeof(struct SModule));
    drv.mod_isys.mod  = malloc(DRV_MAX_MOD * sizeof(struct SModule)); // NOTE: never freed, but we don't care
    memset(drv.mod_isys.mod,  0, DRV_MAX_MOD * sizeof(struct SModule));
    drv.mod_lsys.mod  = malloc(DRV_MAX_MOD * sizeof(struct SModule)); // NOTE: never freed, but we don't care
    memset(drv.mod_lsys.mod,  0, DRV_MAX_MOD * sizeof(struct SModule));
    drv.fake.fake = malloc(MODULE_SETTINGS_MAX_FAKE_COUNT * sizeof(struct FakeModule)); // NOTE: never freed, but we don't care
    memset(drv.fake.fake, 0, MODULE_SETTINGS_MAX_FAKE_COUNT * sizeof(struct FakeModule));

    // Open and parse file
    snprintf(filename, 256, "config/drv-%s.toml", driver);
    fp = fopen(filename, "r");
    if (!fp) {
        printf("ERROR: %s: failed to open\n", filename);
        goto err_exit;
    }
    tbl_root = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);
    if (!tbl_root) {
        printf("ERROR: %s: parse error: %s\n", filename, errbuf);
        goto err_exit;
    }

    // Get values in root table
    v = toml_string_in(tbl_root, "name");
    if (v.ok)
        drv.name = v.u.s; // NOTE: passing ownership of dynamic memory
    v = toml_string_in(tbl_root, "type");
    if (v.ok)
        drv.type = v.u.s; // NOTE: passing ownership of dynamic memory
    v = toml_string_in(tbl_root, "mode");
    if (v.ok)
        drv.mode = v.u.s; // NOTE: passing ownership of dynamic memory

    modlist_add_array(&drv.mod_drv, tbl_root);
    fakelist_add_array(&drv.fake, tbl_root);
    free(tbl_root);

    // Open and parse type ingame
    snprintf(filename, 256, "config/type-%s-ingame.toml", drv.type);
    fp = fopen(filename, "r");
    if (!fp) {
        printf("ERROR: %s: failed to open\n", filename);
        goto err_exit;
    }
    tbl_root = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);
    if (!tbl_root) {
        printf("ERROR: %s: parse error: %s\n", filename, errbuf);
        goto err_exit;
    }
    modlist_add_array(&drv.mod_isys, tbl_root);
    fakelist_add_array(&drv.fake, tbl_root);
    free(tbl_root);

    // Open and parse type load
    snprintf(filename, 256, "config/type-%s-load.toml", drv.type);
    fp = fopen(filename, "r");
    if (!fp) {
        printf("ERROR: %s: failed to open\n", filename);
        goto err_exit;
    }
    tbl_root = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);
    if (!tbl_root) {
        printf("ERROR: %s: parse error: %s\n", filename, errbuf);
        goto err_exit;
    }
    modlist_add_array(&drv.mod_lsys, tbl_root);
    //fakelist_add_array(&drv.fake, tbl_root);
    free(tbl_root);

    return 0;

err_exit:
    if (tbl_root != NULL)
        free(tbl_root);

    return -1;
}

int load_system()
{
    int i;
    FILE* fp;
    char errbuf[200];
    toml_table_t *tbl_root = NULL;
    toml_array_t *arr;
    toml_datum_t v;

    // Initialize system structure
    memset(&sys, 0, sizeof(struct SSystemSettings));

    // Open and parse file
    fp = fopen("config/system.toml", "r");
    if (!fp) {
        printf("ERROR: %s: failed to open\n", "system.toml");
        goto err_exit;
    }
    tbl_root = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);
    if (!tbl_root) {
        printf("ERROR: %s: parse error: %s\n", "system.toml", errbuf);
        goto err_exit;
    }

    arr = toml_array_in(tbl_root, "ilink_id");
    if (arr != NULL) {
        if (toml_array_nelem(arr) == 8) {
            for (i=0; i < 8; i++) {
                v = toml_int_at(arr, i);
                if (v.ok)
                    sys.ilink_id[i] = v.u.i;
            }
        }
        free(arr);
    }

    arr = toml_array_in(tbl_root, "disk_id");
    if (arr != NULL) {
        if (toml_array_nelem(arr) == 5) {
            for (i=0; i < 5; i++) {
                v = toml_int_at(arr, i);
                if (v.ok)
                    sys.disk_id[i] = v.u.i;
            }
        }
        free(arr);
    }

    free(tbl_root);

    return 0;

err_exit:
    if (tbl_root != NULL)
        free(tbl_root);

    return -1;
}

int main(int argc, char *argv[])
{
    irxtab_t *irxtable;
    irxptr_t *irxptr_tab;
    u8 *irxptr;
    int fd_iso = 0;
    int fd_system_cnf;
    int i;
    void *eeloadCopy, *initUserMemory;
    const char *sGameID;
    uint32_t layer1_lba_start = 0;
    off_t iso_size = 0;
    struct cdvdman_settings_bdm *settings = NULL;

    printf("----------------------------\n");
    printf("- Neutrino PS2 Game Loader -\n");
    printf("-       By Maximus32       -\n");
    printf("----------------------------\n");

    if (argc < 2) {
        printf("ERROR: no arguments provided\n");
        print_usage();
        return -1;
    }

    const char *sDriver = NULL;
    const char *sFileNameISO = NULL;
    const char *sFileNameELF = NULL;
    const char *sMediaType = NULL;
    const char *sCompat = NULL;
    u32 iCompat = 0;
    enum SCECdvdMediaType eMediaType = SCECdNODISC;
    int iEnableDebugColors = 0;
    for (i=1; i<argc; i++) {
        //printf("argv[%d] = %s\n", i, argv[i]);
        if (!strncmp(argv[i], "-drv=", 5))
            sDriver = &argv[i][5];
        else if (!strncmp(argv[i], "-iso=", 5))
            sFileNameISO = &argv[i][5];
        else if (!strncmp(argv[i], "-elf=", 5))
            sFileNameELF = &argv[i][5];
        else if (!strncmp(argv[i], "-mt=", 4))
            sMediaType = &argv[i][4];
        else if (!strncmp(argv[i], "-gc", 3))
            sCompat = &argv[i][4];
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

    /*
     * Load system settings
     */
    if (load_system() < 0) {
        printf("ERROR: failed to load system settings\n");
        return -1;
    }

    /*
     * Figure out what drivers we need
     */
    if (load_driver(sDriver) < 0) {
        printf("ERROR: driver %s failed\n", sDriver);
        return -1;
    }
    printf("Driver:  %s\n", drv.name);
    printf("Type:    %s\n", drv.type);
    printf("Mode:    %s\n", drv.mode);

    /*
     * Load all needed files before rebooting the IOP
     */
    if (module_load(&mod_ee_core) < 0)
        return -1;
    if (modlist_load(&drv.mod_isys) < 0)
        return -1;
    if (modlist_load(&drv.mod_lsys) < 0)
        return -1;
    if (modlist_load(&drv.mod_drv) < 0)
        return -1;

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

    /*
     * Start load environment modules
     */
    if (modlist_start(&drv.mod_lsys) < 0)
        return -1;
    if (modlist_start(&drv.mod_drv) < 0)
        return -1;

    /*
     * If we load a replacement CDVDMAN then we assume we're not using the DVD drive
     * but an ISO file instead.
     */
    int iUseDrive = (modlist_get_by_udnlname(&drv.mod_isys, "CDVDMAN") == NULL) ? 1 : 0;
    if (iUseDrive == 0) {
        if (modlist_get_by_name(&drv.mod_lsys, "fileXio.irx") != NULL)
            fileXioInit();

        /*
         * Check if file exists
         * Give low level drivers 10s to start
         */
        printf("Loading %s...\n", sFileNameISO);
        for (i = 0; i < 1000; i++) {
            fd_iso = open(sFileNameISO, O_RDONLY);
            if (fd_iso >= 0)
                break;

            // Give low level drivers some time to init
            nopdelay();
        }
        if (fd_iso < 0) {
            printf("Unable to open %s\n", sFileNameISO);
            return -1;
        }
        // Get ISO file size
        iso_size = lseek64(fd_iso, 0, SEEK_END);
        char buffer[6];
        // Validate this is an ISO
        lseek64(fd_iso, 16 * 2048, SEEK_SET);
        if (read(fd_iso, buffer, sizeof(buffer)) != sizeof(buffer)) {
            printf("Unable to read ISO\n");
            return -1;
        }
        if ((buffer[0x00] != 1) || (strncmp(&buffer[0x01], "CD001", 5))) {
            printf("File is not a valid ISO\n");
            return -1;
        }
        // Get ISO layer0 size
        uint32_t layer0_lba_size;
        lseek64(fd_iso, 16 * 2048 + 80, SEEK_SET);
        if (read(fd_iso, &layer0_lba_size, sizeof(layer0_lba_size)) != sizeof(layer0_lba_size)) {
            printf("ISO invalid\n");
            return -1;
        }
        // Try to get ISO layer1 size
        layer1_lba_start = 0;
        lseek64(fd_iso, (uint64_t)layer0_lba_size * 2048, SEEK_SET);
        if (read(fd_iso, buffer, sizeof(buffer)) == sizeof(buffer)) {
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
         * Mount as ISO so we can get ELF name to boot
         */
        int fd_isomount = fileXioMount("iso:", sFileNameISO, FIO_MT_RDONLY);
        if (fd_isomount < 0) {
            printf("ERROR: Unable to mount %s as iso\n", sFileNameISO);
            return -1;
        }

        fd_system_cnf = open("iso:\\SYSTEM.CNF;1", O_RDONLY);
    }
    else {
        fd_system_cnf = open("cdrom:\\SYSTEM.CNF;1", O_RDONLY);
    }

    if (fd_system_cnf < 0) {
        printf("ERROR: Unable to open SYSTEM.CNF from disk\n");
        return -1;
    }
    char config_data[128];
    read(fd_system_cnf, config_data, 128);
    char *fname_start = strstr(config_data, "cdrom0:");
    char *fname_end = strstr(config_data, ";1");
    if (fname_start == NULL || fname_end == NULL) {
        printf("ERROR: file name not found in SYSTEM.CNF\n");
        return -1;
    }
    sGameID = &fname_start[8];
    fname_end[0] = '\0';
    printf("config name: %s\n", sGameID);
    close(fd_system_cnf);

    if (iUseDrive == 0)
        fileXioUmount("iso:");

    if (sFileNameELF == NULL)
        sFileNameELF = sGameID;

    ResetDeckardXParams();
    ApplyDeckardXParam(sGameID);

    if (iUseDrive == 0) {
        //
        // Locate and set cdvdman settings
        //
        struct SModule *mod_cdvdman = modlist_get_by_udnlname(&drv.mod_isys, "CDVDMAN");
        for (i = 0; i < mod_cdvdman->iSize; i += 4) {
            if (*(u32 *)(mod_cdvdman->pData + i) == MODULE_SETTINGS_MAGIC) {
                settings = (struct cdvdman_settings_bdm *)(mod_cdvdman->pData + i);
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
        if (sys.ilink_id_int != 0) {
            printf("Overriding i.Link ID: %2x %2x %2x %2x %2x %2x %2x %2x\n"
            , sys.ilink_id[0]
            , sys.ilink_id[1]
            , sys.ilink_id[2]
            , sys.ilink_id[3]
            , sys.ilink_id[4]
            , sys.ilink_id[5]
            , sys.ilink_id[6]
            , sys.ilink_id[7]);
            settings->common.ilink_id_int = sys.ilink_id_int;
        }
        if (sys.disk_id_int != 0) {
            printf("Using disk ID: %2x %2x %2x %2x %2x\n"
            , sys.disk_id[0]
            , sys.disk_id[1]
            , sys.disk_id[2]
            , sys.disk_id[3]
            , sys.disk_id[4]);
            settings->common.disk_id_int = sys.disk_id_int;
        }

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
        iso_frag->frag_count = fileXioIoctl2(fd_iso, USBMASS_IOCTL_GET_FRAGLIST, NULL, 0, (void *)&settings->frags[iso_frag->frag_start], sizeof(bd_fragment_t) * (BDM_MAX_FRAGS - iso_frag->frag_start));
        iso_frag->size       = iso_size;
        printf("ISO fragments: start=%u, count=%u\n", iso_frag->frag_start, iso_frag->frag_count);
        for (i=0; i<iso_frag->frag_count; i++) {
            printf("- frag[%d] start=%u, count=%u\n", i, (u32)settings->frags[iso_frag->frag_start+i].sector, settings->frags[iso_frag->frag_start+i].count);
        }
        if ((iso_frag->frag_start + iso_frag->frag_count) > BDM_MAX_FRAGS) {
            printf("Too many fragments (%d)\n", iso_frag->frag_start + iso_frag->frag_count);
            return -1;
        }
        settings->drvName = (u32)fileXioIoctl2(fd_iso, USBMASS_IOCTL_GET_DRIVERNAME, NULL, 0, NULL, 0);
        fileXioIoctl2(fd_iso, USBMASS_IOCTL_GET_DEVICE_NUMBER, NULL, 0, &settings->devNr, 4);
        char *drvName = (char *)&settings->drvName;
        printf("Using BDM device: %s%d\n", drvName, settings->devNr);
        close(fd_iso);
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
    memset((void *)0x00084000, 0, 0x00100000 - 0x00084000);
#pragma GCC diagnostic pop

    // Count the number of modules to pass to the ee_core
    int modcount = 1; // IOPRP
    modcount += drv.mod_drv.count;
    for (i = 0; i < drv.mod_isys.count; i++) {
        if (drv.mod_isys.mod[i].sUDNL == NULL)
            modcount++;
    }

    irxtable = (irxtab_t *)get_modstorage(sGameID);
    if (irxtable == NULL)
        irxtable = (irxtab_t *)OPL_MOD_STORAGE;
    irxptr_tab = (irxptr_t *)((unsigned char *)irxtable + sizeof(irxtab_t));
    irxptr = (u8 *)((((unsigned int)irxptr_tab + sizeof(irxptr_t) * modcount) + 0xF) & ~0xF);

    irxtable->modules = irxptr_tab;
    irxtable->count = 0;

    struct fakemod_data *fmd = NULL;
    struct SModule *mod_fakemod = modlist_get_by_name(&drv.mod_isys, "fakemod.irx");
    if (mod_fakemod != NULL) {
        for (i = 0; i < mod_fakemod->iSize; i += 4) {
            if (*(u32 *)(mod_fakemod->pData + i) == MODULE_SETTINGS_MAGIC) {
                fmd = (struct fakemod_data *)(mod_fakemod->pData + i);
                break;
            }
        }
    }

    if (fmd != NULL) {
        size_t stringbase = 0;
        printf("Faking modules:\n");
        for (i = 0; i < drv.fake.count; i++) {
            size_t len;

            printf("- %s, %s\n", drv.fake.fake[i].fname, drv.fake.fake[i].name);

            // Copy file name into cdvdman data
            len = strlen(drv.fake.fake[i].fname) + 1;
            if ((stringbase + len) > MODULE_SETTINGS_MAX_DATA_SIZE) {
                printf("Too much fake string data\n");
                return -1;
            }
            strcpy((char *)&fmd->data[stringbase], drv.fake.fake[i].fname);
            fmd->fake[i].fname = (char *)(stringbase + 0x80000000);
            stringbase += len;

            // Copy module name into cdvdman data
            len = strlen(drv.fake.fake[i].name) + 1;
            if ((stringbase + len) > MODULE_SETTINGS_MAX_DATA_SIZE) {
                printf("Too much fake string data\n");
                return -1;
            }
            strcpy((char *)&fmd->data[stringbase], drv.fake.fake[i].name);
            fmd->fake[i].name = (char *)(stringbase + 0x80000000);
            stringbase += len;

            fmd->fake[i].id          = 0xdead0 + i;
            fmd->fake[i].prop        = drv.fake.fake[i].prop;
            fmd->fake[i].version     = drv.fake.fake[i].version;
            fmd->fake[i].returnValue = drv.fake.fake[i].returnValue;
        }
    }

    //
    // Patch IOPRP.img with our custom modules
    //
    //printf("IOPRP.img (old):\n");
    //print_romdir(ioprp_img_base.romdir);
    unsigned int ioprp_size;

    if (iUseDrive == 0)
        ioprp_size = patch_IOPRP_image((struct romdir_entry *)irxptr, ioprp_img_full.romdir);
    else
        ioprp_size = patch_IOPRP_image((struct romdir_entry *)irxptr, ioprp_img_dvd.romdir);
    //printf("IOPRP.img (new):\n");
    //print_romdir((struct romdir_entry *)irxptr);
    irxptr_tab->size = ioprp_size;
    irxptr_tab->ptr = irxptr;
    irxptr_tab++;
    irxptr += ioprp_size;
    irxtable->count++;

    //
    // Load modules into place
    //
    for (i = 0; i < drv.mod_isys.count; i++) {
        // Load only the modules that are not part of IOPRP / UDNL
        if (drv.mod_isys.mod[i].sUDNL == NULL) {
            irxptr = module_install(&drv.mod_isys.mod[i], irxptr, irxptr_tab++);
            irxtable->count++;
        }
    }
    for (i = 0; i < drv.mod_drv.count; i++) {
        irxptr = module_install(&drv.mod_drv.mod[i], irxptr, irxptr_tab++);
        irxtable->count++;
    }

    //
    // Load EECORE ELF sections
    //
    u8 *boot_elf = (u8 *)mod_ee_core.pData;
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
    eecc_setGameMode(&eeconf, drv.mode);
    eecc_setKernelConfig(&eeconf, (u32)eeloadCopy, (u32)initUserMemory);
    eecc_setModStorageConfig(&eeconf, (u32)irxtable, (u32)irxptr);
    eecc_setGameID(&eeconf, sGameID);
    eecc_setFileName(&eeconf, sFileNameELF);
    eecc_setCompatFlags(&eeconf, iCompat);
    eecc_setDebugColors(&eeconf, iEnableDebugColors ? true : false);
    printf("Starting ee_core with following arguments:\n");
    eecc_print(&eeconf);

    ExecPS2((void *)eh->entry, NULL, eecc_argc(&eeconf), (char **)eecc_argv(&eeconf));

    return 0;
}
