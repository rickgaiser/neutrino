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
#include <stdint.h>
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
#include "../../../iop/common/fhi_bdm.h"
#include "../../../iop/common/fhi.h"
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
    printf("Usage: neutrino.elf options\n");
    printf("\n");
    printf("Options:\n");
    printf("  -bsd=<driver>     Backing store drivers, supported are:\n");
    printf("                    - no (default)\n");
    printf("                    - ata\n");
    printf("                    - ata-hdl\n");
    printf("                    - usb\n");
    printf("                    - mx4sio\n");
    printf("                    - udpbd\n");
    printf("                    - ilink\n");
    printf("\n");
    printf("  -dvd=<mode>       DVD emulation mode, supported are:\n");
    printf("                    - no (default)\n");
    printf("                    - esr\n");
    printf("                    - <file>\n");
    printf("\n");
    printf("  -ata0=<mode>      ATA HDD 0 emulation mode, supported are:\n");
    printf("                    - no (default)\n");
    printf("                    - <file>\n");
    printf("                    NOTE: only both emulated, or both real.\n");
    printf("                          mixing not possible\n");
    printf("  -ata0id=<mode>    ATA 0 HDD ID emulation mode, supported are:\n");
    printf("                    - no (default)\n");
    printf("                    - <file>\n");
    printf("                    NOTE: only supported if ata0 is present.\n");
    printf("  -ata1=<mode>      See -ata0=<mode>\n");
    printf("\n");
    printf("  -mc0=<mode>       MC0 emulation mode, supported are:\n");
    printf("                    - no (default)\n");
    printf("                    - <file>\n");
    printf("  -mc1=<mode>       See -mc0=<mode>\n");
    printf("\n");
    printf("  -elf=<file>       ELF file to boot, supported are:\n");
    printf("                    - auto (elf file from cd/dvd) (default)\n");
    printf("                    - <file>\n");
    printf("\n");
    printf("  -mt=<type>        Select media type, supported are:\n");
    printf("                    - cd\n");
    printf("                    - dvd\n");
    printf("                    Defaults to cd for size<=650MiB, and dvd for size>650MiB\n");
    printf("\n");
    printf("  -gc=<compat>      Game compatibility modes, supported are:\n");
    printf("                    - 0: Disable builtin compat flags\n");
    printf("                    - 1: IOP: Accurate reads (sceCdRead)\n");
    printf("                    - 2: IOP: Sync reads (sceCdRead)\n");
    printf("                    - 3: EE : Unhook syscalls\n");
    printf("                    - 5: IOP: Emulate DVD-DL\n");
    printf("                    Multiple options possible, for example -gc=23\n");
    printf("\n");
    printf("  -cfg=<file>       Load extra user/game specific config file (without .toml extension)\n");
    printf("\n");
    printf("  -dbc              Enable debug colors\n");
    printf("  -logo             Enable logo (adds rom0:PS2LOGO to arguments)\n");
    printf("\n");
    printf("  --b               Break, all following parameters are passed to the ELF\n");
    printf("\n");
    printf("Usage examples:\n");
    printf("  neutrino.elf -bsd=usb -dvd=mass:path/to/filename.iso\n");
    printf("  neutrino.elf -bsd=ata -dvd=mass:path/to/filename.iso\n");
    printf("  neutrino.elf -bsd=ata-hdl -dvd=hdl:filename.iso\n");
}

struct SModule
{
    const char *sFileName;
    const char *sUDNL;

    off_t iSize;
    void *pData;

    int arg_len;
    char *args;
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
    char *sBSD;
    char *sDVDMode;
    char *sATA0File;
    char *sATA0IDFile;
    char *sATA1File;
    char *sMC0File;
    char *sMC1File;
    char *sELFFile;
    char *sMT;
    char *sGC;
    char *sCFGFile;
    int bDebugColors;
    int bLogo;

    uint8_t fs_sectors;

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
    // Drivers for load environment
    struct SModList mod_l_env;
    // Drivers for emulation environment
    struct SModList mod_e_env;
    // Drivers for all environments
    struct SModList mod_all_env;
    // Module faking for emulation environment
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

    IRX_ID = SifExecModuleBuffer(mod->pData, mod->iSize, mod->arg_len, mod->args, &rv);
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

static void print_iop_args(int arg_len, const char *args)
{
    // Multiple null terminated strings together
    int args_idx = 0;
    int was_null = 1;

    if (arg_len == 0)
        return;

    printf("Module arguments (arg_len=%d):\n", arg_len);

    // Search strings
    while(args_idx < arg_len) {
        if (args[args_idx] == 0) {
            if (was_null == 1) {
                printf("- args[%d]=0\n", args_idx);
            }
            was_null = 1;
        }
        else if (was_null == 1) {
            printf("- args[%d]='%s'\n", args_idx, &args[args_idx]);
            was_null = 0;
        }
        args_idx++;
    }
}

static uint8_t * module_install(struct SModule *mod, uint8_t *addr, irxptr_t *irx)
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
    irx->arg_len = mod->arg_len;
    memcpy(addr, mod->args, irx->arg_len);
    irx->args = (char *)addr;
    addr += irx->arg_len;

    printf("Module %s installed to 0x%p\n", mod->sFileName, irx->ptr);
    print_iop_args(mod->arg_len, mod->args);

    // Align to 16 bytes
    return (uint8_t *)((uint32_t)(addr + 0xf) & ~0xf);
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
    uint8_t *ioprp_in = (uint8_t *)romdir_in;
    uint8_t *ioprp_out = (uint8_t *)romdir_out;

    while (romdir_in->name[0] != '\0') {
        struct SModule *mod = modlist_get_by_udnlname(&drv.mod_e_env, romdir_in->name);
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

    return (ioprp_out - (uint8_t *)romdir_out_org);
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
    toml_array_t *arr;
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
    arr = toml_array_in(t, "args");
    if (arr != NULL) {
        int i;
        m->args = malloc(256); // NOTE: never freed, but we don't care
        m->arg_len = 0;
        for (i=0; i < toml_array_nelem(arr); i++) {
            v = toml_string_at(arr, i);
            if (v.ok) {
                strcpy(&m->args[m->arg_len], v.u.s);
                m->arg_len += strlen(v.u.s) + 1; // +1 for 0 termination
            }
        }
    }

    return 0;
}

int modlist_add_array(struct SModList *ml, toml_table_t *tbl_root, const char *arrname)
{
    int i;
    toml_array_t *arr = toml_array_in(tbl_root, arrname);
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
    v = toml_bool_in(t, "unload");
    if (v.ok)
        f->prop |= (v.u.b != 0) ? FAKE_PROP_UNLOAD : 0;
    v = toml_int_in(t, "version");
    if (v.ok)
        f->version = v.u.i;
    v = toml_int_in(t, "loadrv");
    if (v.ok)
        f->returnLoad = v.u.i;
    v = toml_int_in(t, "startrv");
    if (v.ok)
        f->returnStart = v.u.i;

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

void driver_init()
{
    // Initialize system structure
    memset(&sys, 0, sizeof(struct SSystemSettings));

    // Initialize driver structure
    memset(&drv, 0, sizeof(struct SDriver));
    drv.mod_all_env.mod  = malloc(DRV_MAX_MOD * sizeof(struct SModule)); // NOTE: never freed, but we don't care
    memset(drv.mod_all_env.mod,  0, DRV_MAX_MOD * sizeof(struct SModule));
    drv.mod_e_env.mod  = malloc(DRV_MAX_MOD * sizeof(struct SModule)); // NOTE: never freed, but we don't care
    memset(drv.mod_e_env.mod,  0, DRV_MAX_MOD * sizeof(struct SModule));
    drv.mod_l_env.mod  = malloc(DRV_MAX_MOD * sizeof(struct SModule)); // NOTE: never freed, but we don't care
    memset(drv.mod_l_env.mod,  0, DRV_MAX_MOD * sizeof(struct SModule));
    drv.fake.fake = malloc(MODULE_SETTINGS_MAX_FAKE_COUNT * sizeof(struct FakeModule)); // NOTE: never freed, but we don't care
    memset(drv.fake.fake, 0, MODULE_SETTINGS_MAX_FAKE_COUNT * sizeof(struct FakeModule));
}

void toml_string_in_overwrite(toml_table_t *t, const char *name, char **dest)
{
    toml_datum_t v = toml_string_in(t, name);
    if (v.ok) {
        if (*dest != NULL)
            free (*dest);
        *dest = v.u.s;
    }
}

void toml_bool_in_overwrite(toml_table_t *t, const char *name, int *dest)
{
    toml_datum_t v = toml_bool_in(t, name);
    if (v.ok)
        *dest = v.u.b;
}

int load_driver(const char * type, const char * subtype)
{
    FILE* fp;
    char filename[256];
    char errbuf[200];
    toml_table_t *tbl_root = NULL;
    toml_array_t *arr;
    toml_datum_t v;

    // Open and parse file
    if (subtype != NULL)
        snprintf(filename, 256, "config/%s-%s.toml", type, subtype);
    else
        snprintf(filename, 256, "config/%s.toml", type);
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

    // Recurse into dependencies
    v = toml_string_in(tbl_root, "depends");
    if (v.ok) {
        load_driver(v.u.s, NULL);
        free(v.u.s);
    }

    // Display driver set being loaded
    v = toml_string_in(tbl_root, "name");
    if (v.ok) {
        printf("Loading: %s\n", v.u.s);
        free(v.u.s);
    }

    toml_string_in_overwrite(tbl_root, "default_bsd",    &sys.sBSD);
    toml_string_in_overwrite(tbl_root, "default_dvd",    &sys.sDVDMode);
    toml_string_in_overwrite(tbl_root, "default_ata0",   &sys.sATA0File);
    toml_string_in_overwrite(tbl_root, "default_ata0id", &sys.sATA0IDFile);
    toml_string_in_overwrite(tbl_root, "default_ata1",   &sys.sATA1File);
    toml_string_in_overwrite(tbl_root, "default_mc0",    &sys.sMC0File);
    toml_string_in_overwrite(tbl_root, "default_mc1",    &sys.sMC1File);
    toml_string_in_overwrite(tbl_root, "default_elf",    &sys.sELFFile);
    toml_string_in_overwrite(tbl_root, "default_mt",     &sys.sMT);
    toml_string_in_overwrite(tbl_root, "default_gc",     &sys.sGC);
    toml_string_in_overwrite(tbl_root, "default_cfg",    &sys.sCFGFile);
    toml_bool_in_overwrite  (tbl_root, "default_dbc",    &sys.bDebugColors);
    toml_bool_in_overwrite  (tbl_root, "default_logo",   &sys.bLogo);

    // Number of sectors for fs buffer in cdvdman_emu
    v = toml_int_in(tbl_root, "cdvdman_fs_sectors");
    if (v.ok)
        sys.fs_sectors = v.u.i;

    arr = toml_array_in(tbl_root, "ilink_id");
    if (arr != NULL) {
        if (toml_array_nelem(arr) == 8) {
            int i;
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
            int i;
            for (i=0; i < 5; i++) {
                v = toml_int_at(arr, i);
                if (v.ok)
                    sys.disk_id[i] = v.u.i;
            }
        }
        free(arr);
    }

    modlist_add_array(&drv.mod_all_env, tbl_root, "module");
    modlist_add_array(&drv.mod_e_env, tbl_root, "module-ee");
    modlist_add_array(&drv.mod_l_env, tbl_root, "module-le");
    fakelist_add_array(&drv.fake, tbl_root);
    free(tbl_root);

    return 0;

err_exit:
    if (tbl_root != NULL)
        free(tbl_root);

    return -1;
}

/*
 * Get a pointer to the settings data structure of a module
 */
void *modlist_get_settings(struct SModList *ml, const char *name)
{
    struct SModule *mod = modlist_get_by_name(ml, name);
    void *settings = NULL;

    if (mod != NULL) {
        int i;
        for (i = 0; i < mod->iSize; i += 4) {
            if (*(uint32_t *)(mod->pData + i) == MODULE_SETTINGS_MAGIC) {
                settings = (void *)(mod->pData + i);
                break;
            }
        }
    }

    return settings;
}

int fhi_bdm_add_file_by_fd(struct fhi_bdm *bdm, int fhi_fid, int fd)
{
    int i;
    off_t size;
    unsigned int frag_start = 0;
    struct fhi_bdm_fragfile *frag = &bdm->fragfile[fhi_fid];

    // Get file size
    size = lseek64(fd, 0, SEEK_END);

    // Get current frag use count
    for (i = 0; i < BDM_MAX_FILES; i++)
        frag_start += bdm->fragfile[i].frag_count;

    // Set fragment file
    frag->frag_start = frag_start;
    frag->frag_count = fileXioIoctl2(fd, USBMASS_IOCTL_GET_FRAGLIST, NULL, 0, (void *)&bdm->frags[frag->frag_start], sizeof(bd_fragment_t) * (BDM_MAX_FRAGS - frag->frag_start));
    frag->size = size;

    // Check for max fragments
    if ((frag->frag_start + frag->frag_count) > BDM_MAX_FRAGS) {
        printf("Too many fragments (%d)\n", frag->frag_start + frag->frag_count);
        return -1;
    }

    // Debug info
    printf("fragfile[%d] fragments: start=%u, count=%u\n", fhi_fid, frag->frag_start, frag->frag_count);
    for (i=0; i<frag->frag_count; i++)
        printf("- frag[%d] start=%u, count=%u\n", i, (unsigned int)bdm->frags[frag->frag_start+i].sector, bdm->frags[frag->frag_start+i].count);

    // Set BDM driver name and number
    // NOTE: can be set only once! Check?
    bdm->drvName = (uint32_t)fileXioIoctl2(fd, USBMASS_IOCTL_GET_DRIVERNAME, NULL, 0, NULL, 0);
    fileXioIoctl2(fd, USBMASS_IOCTL_GET_DEVICE_NUMBER, NULL, 0, &bdm->devNr, 4);
    char *drvName = (char *)&bdm->drvName;
    printf("Using BDM device: %s%d\n", drvName, (int)bdm->devNr);

    return 0;
}

int fhi_bdm_add_file(struct fhi_bdm *bdm, int fhi_fid, const char *name)
{
    int i, fd, rv;

    // Open file
    printf("Loading %s...\n", name);
    for (i = 0; i < 1000; i++) {
        fd = open(name, O_RDONLY);
        if (fd >= 0)
            break;

        // Give low level drivers some time to init
        nopdelay();
    }
    if (fd < 0) {
        printf("Unable to open %s\n", name);
        return -1;
    }

    rv = fhi_bdm_add_file_by_fd(bdm, fhi_fid, fd);
    close(fd);
    return rv;
}

int main(int argc, char *argv[])
{
    irxtab_t *irxtable;
    irxptr_t *irxptr_tab;
    uint8_t *irxptr;
    int i;
    void *eeloadCopy, *initUserMemory;
    char sGameID[12];
    int fd_system_cnf;
    char system_cnf_data[128];
    off_t iso_size = 0;

    printf("--------------------------------\n");
    printf("- Neutrino PS2 Device Emulator -\n");
    printf("-         By Maximus32         -\n");
    printf("--------------------------------\n");

    /*
     * Initialiaze main driver structure before filling it from config files
     */
    driver_init();

    /*
     * Load system settings
     */
    if (load_driver("system", NULL) < 0) {
        printf("ERROR: failed to load system settings\n");
        return -1;
    }

    /*
     * Parse user commands
     */
    const char *sDVDFile = NULL;
    const char *sATAMode = "no";
    const char *sMCMode = "no";
    int iELFArgcStart = -1;
    u32 iCompat = 0;
    enum SCECdvdMediaType eMediaType = SCECdNODISC;
    for (i=1; i<argc; i++) {
        //printf("argv[%d] = %s\n", i, argv[i]);
        if (!strncmp(argv[i], "-bsd=", 5))
            sys.sBSD = &argv[i][5];
        else if (!strncmp(argv[i], "-dvd=", 5))
            sys.sDVDMode = &argv[i][5];
        else if (!strncmp(argv[i], "-ata0=", 6))
            sys.sATA0File = &argv[i][6];
        else if (!strncmp(argv[i], "-ata0id=", 8))
            sys.sATA0IDFile = &argv[i][8];
        else if (!strncmp(argv[i], "-ata1=", 6))
            sys.sATA1File = &argv[i][6];
        else if (!strncmp(argv[i], "-mc0=", 5))
            sys.sMC0File = &argv[i][5];
        else if (!strncmp(argv[i], "-mc1=", 5))
            sys.sMC1File = &argv[i][5];
        else if (!strncmp(argv[i], "-elf=", 5))
            sys.sELFFile = &argv[i][5];
        else if (!strncmp(argv[i], "-mt=", 4))
            sys.sMT = &argv[i][4];
        else if (!strncmp(argv[i], "-gc=", 4))
            sys.sGC = &argv[i][4];
        else if (!strncmp(argv[i], "-dbc", 4))
            sys.bDebugColors = true;
        else if (!strncmp(argv[i], "-logo", 5))
            sys.bLogo = true;
        else if (!strncmp(argv[i], "--b", 3)) {
            iELFArgcStart = i + 1;
            break;
        }
        else {
            printf("ERROR: unknown argv[%d] = %s\n", i, argv[i]);
            print_usage();
            return -1;
        }
    }

    /*
     * Load user/game settings
     */
    if (sys.sCFGFile != NULL) {
        if (load_driver(sys.sCFGFile, NULL) < 0) {
            printf("ERROR: failed to load %s\n", sys.sCFGFile);
            return -1;
        }
    }

    // Make sure we don't pass loader arguments to the ELF
    if (iELFArgcStart == -1)
        iELFArgcStart = argc;

    // Check for "file" mode of dvd emulation
    if (strstr(sys.sDVDMode, ":")) {
        sDVDFile = sys.sDVDMode;
        sys.sDVDMode = "file";
    }

    // Check for "file" mode of ata emulation
    if (sys.sATA0File != NULL || sys.sATA1File != NULL) {
        sATAMode = "file";
    }

    // Check for "file" mode of mc emulation
    if (sys.sMC0File != NULL || sys.sMC1File != NULL) {
        sMCMode = "file";
    }

    if (sys.sMT != NULL) {
        if (!strncmp(sys.sMT, "cdda", 4)) {
            eMediaType = SCECdPS2CDDA;
        } else if (!strncmp(sys.sMT, "cd", 2)) {
            eMediaType = SCECdPS2CD;
        } else if (!strncmp(sys.sMT, "dvdv", 4)) {
            eMediaType = SCECdDVDV;
        } else if (!strncmp(sys.sMT, "dvd", 3)) {
            eMediaType = SCECdPS2DVD;
        } else {
            printf("ERROR: media type %s not supported\n", sys.sMT);
            print_usage();
            return -1;
        }
    }

    if (sys.sGC != NULL) {
        while (*sys.sGC != 0) {
            char c = *sys.sGC;
            switch (c) {
                case '0':
                    iCompat |= 1U << 31; // Set dummy flag
                    break;
                case '1':
                case '2':
                case '3':
                case '5':
                    iCompat |= 1U << (c - '1');
                    break;
                default:
                    printf("ERROR: compat flag %c not supported\n", c);
                    print_usage();
                    return -1;
            }
            sys.sGC++;
        }
    }

    /*
     * Load backing store driver settings
     */
    if (!strcmp(sys.sBSD, "no")) {
        // Load nothing
    } else if (load_driver("bsd", sys.sBSD) < 0) {
        printf("ERROR: driver %s failed\n", sys.sBSD);
        return -1;
    }

    /*
     * Load CD/DVD emulation driver settings
     */
    if (!strcmp(sys.sDVDMode, "no")) {
        // Load nothing
    } else if (load_driver("emu-dvd", sys.sDVDMode) < 0) {
        printf("ERROR: dvd driver %s failed\n", sys.sDVDMode);
        return -1;
    }

    /*
     * Load ATA emulation driver settings
     */
    if (!strcmp(sATAMode, "no")) {
        // Load nothing
    } else if (load_driver("emu-ata", sATAMode) < 0) {
        printf("ERROR: ata driver %s failed\n", sATAMode);
        return -1;
    }

    /*
     * Load MC emulation driver settings
     */
    if (!strcmp(sMCMode, "no")) {
        // Load nothing
    } else if (load_driver("emu-mc", sMCMode) < 0) {
        printf("ERROR: mc driver %s failed\n", sMCMode);
        return -1;
    }

    /*
     * Load all needed files before rebooting the IOP
     */
    if (module_load(&mod_ee_core) < 0)
        return -1;
    if (modlist_load(&drv.mod_e_env) < 0)
        return -1;
    if (modlist_load(&drv.mod_l_env) < 0)
        return -1;
    if (modlist_load(&drv.mod_all_env) < 0)
        return -1;

    /*
     * Reboot IOP into Load Environment (LE)
     */
    printf("Reboot IOP into Load Environment (LE)\n");
#if 1
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
#endif

    /*
     * Start load environment modules
     */
    if (modlist_start(&drv.mod_l_env) < 0)
        return -1;
    if (modlist_start(&drv.mod_all_env) < 0)
        return -1;
    if (modlist_get_by_name(&drv.mod_l_env, "fileXio.irx") != NULL)
        fileXioInit();

    // FAKEMOD optional module
    // Only loaded when modules need to be faked
    struct SModule *mod_fakemod = modlist_get_by_udnlname(&drv.mod_e_env, "FAKEMOD");

    // FHI optional module
    // Only loaded when file access is needed
    // NOTE: FHI is an abstraction to FHI_BDM, FHI_FILE, etc...
    struct SModule *mod_fhi = modlist_get_by_udnlname(&drv.mod_e_env, "FHI");

    // Load module settings for fhi_bdm backing store
    struct fhi_bdm *set_fhi_bdm = modlist_get_settings(&drv.mod_e_env, "fhi_bdm.irx");
    if (set_fhi_bdm != NULL)
        memset((void *)set_fhi_bdm, 0, sizeof(struct fhi_bdm));

    // Load module settings for cdvd emulator
    struct cdvdman_settings_common *set_cdvdman = modlist_get_settings(&drv.mod_e_env, "cdvdman_emu.irx");
    if (set_cdvdman != NULL)
        memset((void *)set_cdvdman, 0, sizeof(struct cdvdman_settings_common));

    // Load module settings for module faker
    struct fakemod_data *set_fakemod = modlist_get_settings(&drv.mod_e_env, "fakemod.irx");
    if (set_fakemod != NULL)
        memset((void *)set_fakemod, 0, sizeof(struct fakemod_data));

    /*
     * Enable DVD emulation
     */
    if (sDVDFile != NULL) {
        uint32_t layer1_lba_start = 0;
        int fd_iso = 0;

        if (set_fhi_bdm == NULL || mod_fhi == NULL) {
            printf("ERROR: DVD emulator needs FHI backing store!\n");
            return -1;
        }
        if (set_cdvdman == NULL) {
            printf("ERROR: DVD emulator not found!\n");
            return -1;
        }
        // Make sure the FHI module gets loaded
        mod_fhi->sUDNL = NULL;

        /*
         * Check if file exists
         * Give low level drivers 10s to start
         */
        printf("Loading %s...\n", sDVDFile);
        for (i = 0; i < 1000; i++) {
            fd_iso = open(sDVDFile, O_RDONLY);
            if (fd_iso >= 0)
                break;

            // Give low level drivers some time to init
            nopdelay();
        }
        if (fd_iso < 0) {
            printf("Unable to open %s\n", sDVDFile);
            return -1;
        }
        // Get ISO file size
        iso_size = lseek64(fd_iso, 0, SEEK_END);
        printf("- size = %dMiB\n", (int)(iso_size / (1024 * 1024)));

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

        if (eMediaType == SCECdNODISC)
            eMediaType = iso_size <= (333000 * 2048) ? SCECdPS2CD : SCECdPS2DVD;

        const char *sMT;
        switch (eMediaType) {
            case SCECdPS2CDDA: sMT = "ps2 cdda";  break;
            case SCECdPS2CD:   sMT = "ps2 cd";    break;
            case SCECdDVDV:    sMT = "dvd video"; break;
            case SCECdPS2DVD:  sMT = "ps2 dvd";   break;
            default:           sMT = "unknown";
        }
        printf("- media = %s\n", sMT);

        if (fhi_bdm_add_file_by_fd(set_fhi_bdm, FHI_FID_CDVD, fd_iso) < 0)
            return -1;
        close(fd_iso);

        set_cdvdman->media = eMediaType;
        set_cdvdman->layer1_start = layer1_lba_start;
        set_cdvdman->fs_sectors = sys.fs_sectors;
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
            set_cdvdman->ilink_id_int = sys.ilink_id_int;
        }
        if (sys.disk_id_int != 0) {
            printf("Using disk ID: %2x %2x %2x %2x %2x\n"
            , sys.disk_id[0]
            , sys.disk_id[1]
            , sys.disk_id[2]
            , sys.disk_id[3]
            , sys.disk_id[4]);
            set_cdvdman->disk_id_int = sys.disk_id_int;
        }
    }
    else {
        if (iCompat != 0)
            printf("WARNING: compatibility cannot be changed without emulating the DVD\n");
        if (eMediaType != SCECdNODISC)
            printf("WARNING: media type cannot be changed without emulating the DVD\n");
        if (sys.ilink_id_int != 0)
            printf("WARNING: ilink_id cannot be changed without emulating the DVD\n");
        if (sys.disk_id_int != 0)
            printf("WARNING: disk_id cannot be changed without emulating the DVD\n");
    }

    /*
     * Figure out the the elf file to start automatically from the SYSTEM.CNF
     */
    if (strcmp(sys.sELFFile, "auto") == 0) {
        if (sDVDFile != NULL) {
            /*
            * Mount as ISO so we can get ELF name to boot
            */
            if (fileXioMount("iso:", sDVDFile, FIO_MT_RDONLY) < 0) {
                printf("ERROR: Unable to mount %s as iso\n", sDVDFile);
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

        // Read file contents
        read(fd_system_cnf, system_cnf_data, 128);

        // Locate and set ELF file name
        sys.sELFFile = strstr(system_cnf_data, "cdrom0:");
        char *fname_end = strstr(system_cnf_data, ";1");
        if (sys.sELFFile == NULL || fname_end == NULL) {
            printf("ERROR: file name not found in SYSTEM.CNF\n");
            return -1;
        }
        fname_end[2] = '\0';

        // Locate and set GameID
        memcpy(sGameID, &sys.sELFFile[8], 11);
        sGameID[11] = '\0';
        close(fd_system_cnf);

        if (sDVDFile != NULL)
            fileXioUmount("iso:");
    }
    else {
        // Manually specifying an ELF file == no GameID
        sGameID[0] = '\0';
    }

    /*
     * Get ELF/game compatibility flags
     */
    if (iCompat == 0)
        iCompat = get_compat(sGameID);
    iCompat &= ~(1U << 31); // Clear dummy flag

    /*
     * Set CDVDMAN compatibility
     */
    if (sDVDFile != NULL) {
        if (iCompat & COMPAT_MODE_1)
            set_cdvdman->flags |= IOPCORE_COMPAT_ACCU_READS;
        if (iCompat & COMPAT_MODE_2)
            set_cdvdman->flags |= IOPCORE_COMPAT_ALT_READ;
        if (iCompat & COMPAT_MODE_5)
            set_cdvdman->flags |= IOPCORE_COMPAT_EMU_DVDDL;

        printf("Compat flags: 0x%X, IOP=0x%X\n", (unsigned int)iCompat, set_cdvdman->flags);
    }

    /*
     * Set deckard compatibility
     */
    ResetDeckardXParams();
    ApplyDeckardXParam(sGameID);

    /*
     * Enable ATA0 emulation
     */
    if (sys.sATA0File != NULL) {
        // Check for FHI backing store
        if (set_fhi_bdm == NULL || mod_fhi == NULL) {
            printf("ERROR: ATA emulator needs FHI backing store!\n");
            return -1;
        }
        // Make sure the FHI module gets loaded
        mod_fhi->sUDNL = NULL;
        // Load fragfile
        if (fhi_bdm_add_file(set_fhi_bdm, FHI_FID_ATA0, sys.sATA0File) < 0)
            return -1;
        // if ata 0 is ok, load HDD ID file
        if (sys.sATA0IDFile != NULL) {
            if (fhi_bdm_add_file(set_fhi_bdm, FHI_FID_ATA0ID, sys.sATA0IDFile) < 0)
                return -1;
        }
    }

    /*
     * Enable ATA1 emulation
     */
    if (sys.sATA1File != NULL) {
        // Check for FHI backing store
        if (set_fhi_bdm == NULL || mod_fhi == NULL) {
            printf("ERROR: ATA emulator needs FHI backing store!\n");
            return -1;
        }
        // Make sure the FHI module gets loaded
        mod_fhi->sUDNL = NULL;
        // Load fragfile
        if (fhi_bdm_add_file(set_fhi_bdm, FHI_FID_ATA1, sys.sATA1File) < 0)
            return -1;
    }

    /*
     * Enable MC0 emulation
     */
    if (sys.sMC0File != NULL) {
        // Check for FHI backing store
        if (set_fhi_bdm == NULL || mod_fhi == NULL) {
            printf("ERROR: ATA emulator needs FHI backing store!\n");
            return -1;
        }
        // Make sure the FHI module gets loaded
        mod_fhi->sUDNL = NULL;
        // Load fragfile
        if (fhi_bdm_add_file(set_fhi_bdm, FHI_FID_MC0, sys.sMC0File) < 0)
            return -1;
    }

    /*
     * Enable MC1 emulation
     */
    if (sys.sMC1File != NULL) {
        // Check for FHI backing store
        if (set_fhi_bdm == NULL || mod_fhi == NULL) {
            printf("ERROR: ATA emulator needs FHI backing store!\n");
            return -1;
        }
        // Make sure the FHI module gets loaded
        mod_fhi->sUDNL = NULL;
        // Load fragfile
        if (fhi_bdm_add_file(set_fhi_bdm, FHI_FID_MC1, sys.sMC1File) < 0)
            return -1;
    }

    printf("ELF file: %s\n", sys.sELFFile);
    printf("GameID:   %s\n", sGameID);

    /*
     * Fill fake module table
     */
    if (drv.fake.count > 0) {
        size_t stringbase = 0;

        if (set_fakemod == NULL || mod_fakemod == NULL) {
            printf("ERROR: fakemod not found!\n");
            return -1;
        }

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
            strcpy((char *)&set_fakemod->data[stringbase], drv.fake.fake[i].fname);
            set_fakemod->fake[i].fname = (char *)(stringbase + 0x80000000);
            stringbase += len;

            // Copy module name into cdvdman data
            len = strlen(drv.fake.fake[i].name) + 1;
            if ((stringbase + len) > MODULE_SETTINGS_MAX_DATA_SIZE) {
                printf("Too much fake string data\n");
                return -1;
            }
            strcpy((char *)&set_fakemod->data[stringbase], drv.fake.fake[i].name);
            set_fakemod->fake[i].name = (char *)(stringbase + 0x80000000);
            stringbase += len;

            set_fakemod->fake[i].id          = 0xdead0 + i;
            set_fakemod->fake[i].prop        = drv.fake.fake[i].prop;
            set_fakemod->fake[i].version     = drv.fake.fake[i].version;
            set_fakemod->fake[i].returnLoad  = drv.fake.fake[i].returnLoad;
            set_fakemod->fake[i].returnStart = drv.fake.fake[i].returnStart;
        }
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
    memset((void *)0x00084000, 0, 0x00100000 - 0x00084000);
#pragma GCC diagnostic pop

    // Count the number of modules to pass to the ee_core
    int modcount = 1; // IOPRP
    modcount += drv.mod_all_env.count;
    for (i = 0; i < drv.mod_e_env.count; i++) {
        if (drv.mod_e_env.mod[i].sUDNL == NULL)
            modcount++;
    }
    if (drv.fake.count > 0)
        modcount++;

    irxtable = (irxtab_t *)get_modstorage(sGameID);
    if (irxtable == NULL)
        irxtable = (irxtab_t *)OPL_MOD_STORAGE;
    irxptr_tab = (irxptr_t *)((unsigned char *)irxtable + sizeof(irxtab_t));
    irxptr = (uint8_t *)((((unsigned int)irxptr_tab + sizeof(irxptr_t) * modcount) + 0xF) & ~0xF);

    irxtable->modules = irxptr_tab;
    irxtable->count = 0;

    //
    // Patch IOPRP.img with our custom modules
    //
    //printf("IOPRP.img (old):\n");
    //print_romdir(ioprp_img_base.romdir);
    unsigned int ioprp_size;
    if (sDVDFile != NULL)
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
    for (i = 0; i < drv.mod_e_env.count; i++) {
        // Load only the modules that are not part of IOPRP / UDNL
        if (drv.mod_e_env.mod[i].sUDNL == NULL) {
            irxptr = module_install(&drv.mod_e_env.mod[i], irxptr, irxptr_tab++);
            irxtable->count++;
        }
    }
    for (i = 0; i < drv.mod_all_env.count; i++) {
        irxptr = module_install(&drv.mod_all_env.mod[i], irxptr, irxptr_tab++);
        irxtable->count++;
    }
    // Load FAKEMOD last, to prevent it from faking our own modules
    if (drv.fake.count > 0) {
        irxptr = module_install(mod_fakemod, irxptr, irxptr_tab++);
        irxtable->count++;
    }

    //
    // Load EECORE ELF sections
    //
    uint8_t *boot_elf = (uint8_t *)mod_ee_core.pData;
    elf_header_t *eh = (elf_header_t *)boot_elf;
    elf_pheader_t *eph = (elf_pheader_t *)(boot_elf + eh->phoff);
    for (i = 0; i < eh->phnum; i++) {
        if (eph[i].type != ELF_PT_LOAD)
            continue;

        void *pdata = (void *)(boot_elf + eph[i].offset);
        memcpy(eph[i].vaddr, pdata, eph[i].filesz);

        if (eph[i].memsz > eph[i].filesz)
            memset((uint8_t *)eph[i].vaddr + eph[i].filesz, 0, eph[i].memsz - eph[i].filesz);
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
    //eecc_setGameMode(&eeconf, "-"); // FIXME: implement or remove
    if (sGameID[0] != 0)
        eecc_setGameID(&eeconf, sGameID);
    eecc_setELFName(&eeconf, sys.sELFFile);
    eecc_setELFArgs(&eeconf, argc-iELFArgcStart, (const char **)&argv[iELFArgcStart]);
    eecc_setKernelConfig(&eeconf, eeloadCopy, initUserMemory);
    eecc_setModStorageConfig(&eeconf, irxtable, irxptr);
    eecc_setCompatFlags(&eeconf, iCompat);
    eecc_setDebugColors(&eeconf, sys.bDebugColors);
    eecc_setPS2Logo(&eeconf, sys.bLogo);
    printf("Starting ee_core with following arguments:\n");
    eecc_print(&eeconf);

    ExecPS2((void *)eh->entry, NULL, eecc_argc(&eeconf), (char **)eecc_argv(&eeconf));

    return 0;
}
