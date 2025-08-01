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
#include <sifrpc.h>
#include <iopheap.h>
#include <sbv_patches.h>

// Neutrino EE_CORE
#include "../../ee_core/include/interface.h"

// Neutrino IOP modules
#include "../../../iop/common/cdvd_config.h"
#include "../../../iop/common/fakemod.h"
#include "../../../iop/common/fhi_bd.h"
#include "../../../iop/common/fhi_bd_defrag.h"
#include "../../../iop/common/fhi_file.h"
#include "../../../iop/common/fhi_fileid.h"
#include "../../../iop/common/fhi.h"

// Neutrino
#include "elf.h"
#include "patch.h"
#include "ioprp.h"
#include "iso_cnf.h"
#include "xparam.h"
#include "tomlc17.h"

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <io_common.h>

DISABLE_PATCHED_FUNCTIONS();      // Disable the patched functionalities
DISABLE_EXTRA_TIMERS_FUNCTIONS(); // Disable the extra functionalities for timers
PS2_DISABLE_AUTOSTART_PTHREAD();  // Disable pthread functionality
void _libcglue_timezone_update() {}; // Disable timezone update
void _libcglue_rtc_update() {}; // Disable rtc update

// TOML helper functions
void toml_string_move(toml_datum_t v, char **dest);
void toml_string_in_overwrite(toml_datum_t t, const char *name, char **dest);
void toml_bool_in_overwrite(toml_datum_t t, const char *name, int *dest);
void toml_int_in_overwrite(toml_datum_t t, const char *name, int *dest);

int load_config_file(const char * type, const char * subtype);

void print_usage()
{
    printf("Usage: neutrino.elf options\n");
    printf("\n");
    printf("Options:\n");
    printf("  -bsd=<driver>     Backing store drivers, supported are:\n");
    printf("                    - no     (uses cdvd, default)\n");
    printf("                    - ata    (block device)\n");
    printf("                    - usb    (block device)\n");
    printf("                    - mx4sio (block device)\n");
    printf("                    - udpbd  (block device)\n");
    printf("                    - ilink  (block device)\n");
    printf("                    - mmce   (file system)\n");
    printf("\n");
    printf("  -bsdfs=<driver>   Backing store fileystem drivers used for block device, supported are:\n");
    printf("                    - exfat (default)\n");
    printf("                    - hdl   (HD Loader)\n");
    printf("                    - bd    (Block Device)\n");
    printf("                    NOTE: Used only for block devices (see -bsd)\n");
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
    printf("  -gc=<compat>      Game compatibility modes, supported are:\n");
    printf("                    - 0: IOP: Fast reads (sceCdRead)\n");
    printf("                    - 1: dummy\n");
    printf("                    - 2: IOP: Sync reads (sceCdRead)\n");
    printf("                    - 3: EE : Unhook syscalls\n");
    printf("                    - 5: IOP: Emulate DVD-DL\n");
    printf("                    - 7: IOP: Fix game buffer overrun\n");
    printf("                    Multiple options possible, for example -gc=23\n");
    printf("\n");
    printf("  -gsm=x:y:z        GS video mode\n");
    printf("\n");
    printf("                    Parameter x = Interlaced field mode\n");
    printf("                    A full height buffer is used by the game for displaying. Force video output to:\n");
    printf("                    -      : don't force (default)  (480i/576i)\n");
    printf("                    - fp   : force progressive scan (480p/576p)\n");
    printf("\n");
    printf("                    Parameter y = Interlaced frame mode\n");
    printf("                    A half height buffer is used by the game for displaying. Force video output to:\n");
    printf("                    -      : don't force (default)  (480i/576i)\n");
    printf("                    - fp1  : force progressive scan (240p/288p)\n");
    printf("                    - fp2  : force progressive scan (480p/576p line doubling)\n");
    printf("\n");
    printf("                    Parameter z = Compatibility mode\n");
    printf("                    -      : no compatibility mode (default)\n");
    printf("                    - 1    : field flipping type 1 (GSM/OPL)\n");
    printf("                    - 2    : field flipping type 2\n");
    printf("                    - 3    : field flipping type 3\n");
    printf("\n");
    printf("                    Examples:\n");
    printf("                    -gsm=fp       - recommended mode\n");
    printf("                    -gsm=fp::1    - recommended mode, with compatibility 1\n");
    printf("                    -gsm=fp:fp1:2 - all parameters\n");
    printf("\n");
    printf("  -cwd=<path>       Change working directory\n");
    printf("\n");
    printf("  -cfg=<file>       Load extra user/game specific config file (without .toml extension)\n");
    printf("\n");
    printf("  -dbc              Enable debug colors\n");
    printf("  -logo             Enable logo (adds rom0:PS2LOGO to arguments)\n");
    printf("  -qb               Quick-Boot directly into load environment\n");
    printf("\n");
    printf("  --b               Break, all following parameters are passed to the ELF\n");
    printf("\n");
    printf("Usage examples:\n");
    printf("  neutrino.elf -bsd=usb    -dvd=mass:path/to/filename.iso\n");
    printf("  neutrino.elf -bsd=mx4sio -dvd=mass:path/to/filename.iso\n");
    printf("  neutrino.elf -bsd=mmce   -dvd=mmce:path/to/filename.iso\n");
    printf("  neutrino.elf -bsd=ilink  -dvd=mass:path/to/filename.iso\n");
    printf("  neutrino.elf -bsd=udpbd  -dvd=mass:path/to/filename.iso\n");
    printf("  neutrino.elf -bsd=ata    -dvd=mass:path/to/filename.iso\n");
    printf("  neutrino.elf -bsd=ata    -dvd=hdl:filename.iso -bsdfs=hdl\n");
    printf("  neutrino.elf -bsd=udpbd  -dvd=bdfs:udp0p0      -bsdfs=bd\n");
}

#define MOD_ENV_LE (1<<0)
#define MOD_ENV_EE (1<<1)
struct SModule
{
    char *sFileName;
    char *sUDNL;
    char *sFunc;

    off_t iSize;
    void *pData;

    int arg_len;
    char *args;

    unsigned int env;
};

#define DRV_MAX_MOD 20
struct SModList {
    int count;
    struct SModule mod[DRV_MAX_MOD];
};

struct SFakeList {
    int count;
    struct FakeModule fake[MODULE_SETTINGS_MAX_FAKE_COUNT];
};

struct SSystemSettings {
    char *sBSD;
    char *sBSDFS;
    char *sDVDMode;
    char *sATA0File;
    char *sATA0IDFile;
    char *sATA1File;
    char *sMC0File;
    char *sMC1File;
    char *sELFFile;
    char *sGC;
    char *sGSM;
    char *sCFGFile;
    int bDebug;
    int bLogo;
    int bQuickBoot;

    struct {
        // CDVDMAN settings
        char *media_type;
        uint32_t flags;
        int fs_sectors;
        union {
            uint8_t ilink_id[8];
            uint64_t ilink_id_int;
        };
        union {
            uint8_t disk_id[5];
            uint64_t disk_id_int; // 8 bytes, but that's ok for compare reasons
        };
    } cdvdman;

    struct {
        // EECORE settings
        uint32_t flags;
        int iop_reboot_mode[2];
        char *elf;
        int mod_base;
    } eecore;
} sys;

struct SDriver {
    // All modules
    struct SModList mod;
    // List of fake modules for emulation environment
    struct SFakeList fake;
} drv;

struct SModule mod_ee_core;

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

int modlist_load(struct SModList *ml, unsigned int filter)
{
    int i;

    for (i = 0; i < ml->count; i++) {
        if (ml->mod[i].env & filter) {
            if (module_load(&ml->mod[i]) < 0)
                return -1;
        }
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

struct SModule *modlist_get_by_func(struct SModList *ml, const char *func)
{
    int i;

    for (i = 0; i < ml->count; i++) {
        struct SModule *m = &ml->mod[i];
        if (m->sFunc != NULL) {
            if (strcmp(m->sFunc, func) == 0)
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
        return addr;
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
        struct SModule *mod = modlist_get_by_udnlname(&drv.mod, romdir_in->name);
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

int modlist_add(struct SModList *ml, toml_datum_t t)
{
    toml_datum_t v;
    toml_datum_t arr;
    struct SModule *m;

    v = toml_get(t, "file");
    if (v.type != TOML_STRING) {
        printf("ERROR: module.file does not exist\n");
        return -1;
    }

    m = modlist_get_by_name(ml, v.u.s);
    if (m != NULL) {
        printf("WARNING: module %s already loaded\n", m->sFileName);
        // Free dynamic memory
        if (m->sFileName)
            free(m->sFileName);
        if (m->sUDNL)
            free(m->sUDNL);
        if (m->sFunc)
            free(m->sFunc);
        if (m->args)
            free(m->args);
        if (m->pData)
            free(m->pData);
        // Clear entry
        memset(m, 0, sizeof(struct SModule));
    } else {
        if (ml->count >= DRV_MAX_MOD) {
            printf("ERROR: too many modules\n");
            return -1;
        }
        m = &ml->mod[ml->count];
        ml->count++;
    }

    toml_string_move(v, &m->sFileName);

    toml_string_in_overwrite(t, "ioprp", &m->sUDNL);
    toml_string_in_overwrite(t, "func",  &m->sFunc);
    arr = toml_get(t, "args");
    if (arr.type == TOML_ARRAY) {
        int i;
        m->args = malloc(256); // NOTE: never freed, but we don't care
        m->arg_len = 0;
        for (i=0; i < arr.u.arr.size; i++) {
            v = arr.u.arr.elem[i];
            if (v.type == TOML_STRING) {
                strcpy(&m->args[m->arg_len], v.u.s);
                m->arg_len += strlen(v.u.s) + 1; // +1 for 0 termination
            }
        }
    }
    arr = toml_get(t, "env");
    if (arr.type == TOML_ARRAY) {
        int i;
        for (i=0; i < arr.u.arr.size; i++) {
            v = arr.u.arr.elem[i];
            if (v.type == TOML_STRING) {
                if (strncmp(v.u.s, "LE", 2) == 0)
                    m->env |= MOD_ENV_LE;
                else if (strncmp(v.u.s, "EE", 2) == 0)
                    m->env |= MOD_ENV_EE;
                else
                    printf("ERROR: unknown module.env: %s\n", v.u.s);
            }
        }
    }

    return 0;
}

int modlist_add_array(struct SModList *ml, toml_datum_t t)
{
    int i;
    toml_datum_t arr = toml_get(t, "module");
    if (arr.type != TOML_ARRAY)
        return 0;

    for (i=0; i < arr.u.arr.size; i++) {
        toml_datum_t t = arr.u.arr.elem[i];
        if (t.type != TOML_TABLE)
            return -1;
        if (modlist_add(ml, t) < 0)
            return -1;
    }

    return 0;
}

int fakelist_add(struct SFakeList *fl, toml_datum_t t)
{
    toml_datum_t v;
    struct FakeModule *f;

    if (fl->count >= MODULE_SETTINGS_MAX_FAKE_COUNT)
        return -1;
    f = &fl->fake[fl->count];
    fl->count++;

    toml_string_in_overwrite(t, "file", &f->fname);
    toml_string_in_overwrite(t, "name", &f->name);
    v = toml_get(t, "unload");
    if (v.type == TOML_BOOLEAN)
        f->prop |= (v.u.boolean != 0) ? FAKE_PROP_UNLOAD : 0;
    v = toml_get(t, "version");
    if (v.type == TOML_INT64)
        f->version = v.u.int64;
    v = toml_get(t, "loadrv");
    if (v.type == TOML_INT64)
        f->returnLoad = v.u.int64;
    v = toml_get(t, "startrv");
    if (v.type == TOML_INT64)
        f->returnStart = v.u.int64;

    return 0;
}

int fakelist_add_array(struct SFakeList *fl, toml_datum_t t)
{
    int i;
    toml_datum_t arr = toml_get(t, "fake");
    if (arr.type != TOML_ARRAY)
        return 0;

    for (i=0; i < arr.u.arr.size; i++) {
        toml_datum_t t = arr.u.arr.elem[i];
        if (t.type != TOML_TABLE)
            return -1;
        if (fakelist_add(fl, t) < 0)
            return -1;
    }

    return 0;
}

void toml_string_move(toml_datum_t v, char **dest)
{
    // Free old string if previously set
    if (*dest != NULL) {
        free(*dest);
        *dest = NULL;
    }
    // Allocate data for new string
    *dest = malloc(strlen(v.u.s) + 1);
    // Copy string
    strcpy(*dest, v.u.s);
}

void toml_string_in_overwrite(toml_datum_t t, const char *name, char **dest)
{
    toml_datum_t v = toml_get(t, name);
    if (v.type == TOML_STRING)
        toml_string_move(v, dest);
}

void toml_bool_in_overwrite(toml_datum_t t, const char *name, int *dest)
{
    toml_datum_t v = toml_get(t, name);
    if (v.type == TOML_BOOLEAN)
        *dest = v.u.boolean;
}

void toml_int_in_overwrite(toml_datum_t t, const char *name, int *dest)
{
    toml_datum_t v = toml_get(t, name);
    if (v.type == TOML_INT64)
        *dest = v.u.int64;
}

int load_config_eecore(toml_datum_t t)
{
    toml_datum_t arr;
    toml_datum_t v;

    toml_string_in_overwrite(t, "elf",      &sys.eecore.elf);
    toml_int_in_overwrite   (t, "mod_base", &sys.eecore.mod_base);

    arr = toml_get(t, "irm");
    if (arr.type == TOML_ARRAY && arr.u.arr.size == 2) {
        int i;
        for (i=0; i < arr.u.arr.size; i++) {
            v = arr.u.arr.elem[i];
            if (v.type == TOML_INT64)
                sys.eecore.iop_reboot_mode[i] = v.u.int64;
        }
    }

    arr = toml_get(t, "flags");
    if (arr.type == TOML_ARRAY) {
        int i;
        for (i=0; i < arr.u.arr.size; i++) {
            v = arr.u.arr.elem[i];
            if (v.type == TOML_STRING) {
                if(!strcmp(v.u.s, "UNHOOK")) sys.eecore.flags |= EECORE_FLAG_UNHOOK;
            }
        }
    }

    return 0;
}

int load_config_cdvdman(toml_datum_t t)
{
    toml_datum_t arr;
    toml_datum_t v;

    toml_string_in_overwrite(t, "media_type", &sys.cdvdman.media_type);
    toml_int_in_overwrite(t, "fs_sectors", &sys.cdvdman.fs_sectors);

    arr = toml_get(t, "flags");
    if (arr.type == TOML_ARRAY) {
        int i;
        for (i=0; i < arr.u.arr.size; i++) {
            v = arr.u.arr.elem[i];
            if (v.type == TOML_STRING) {
                if(!strcmp(v.u.s, "FAST_READ")) sys.cdvdman.flags |= CDVDMAN_COMPAT_FAST_READ;
                if(!strcmp(v.u.s, "SYNC_READ")) sys.cdvdman.flags |= CDVDMAN_COMPAT_SYNC_READ;
                if(!strcmp(v.u.s, "SYNC_CB"))   sys.cdvdman.flags |= CDVDMAN_COMPAT_SYNC_CALLBACK;
                if(!strcmp(v.u.s, "DVD_DL"))    sys.cdvdman.flags |= CDVDMAN_COMPAT_DVD_DL;
                if(!strcmp(v.u.s, "F1_2001"))   sys.cdvdman.flags |= CDVDMAN_COMPAT_F1_2001;
            }
        }
    }

    arr = toml_get(t, "ilink_id");
    if (arr.type == TOML_ARRAY && arr.u.arr.size == 8) {
        int i;
        for (i=0; i < 8; i++) {
            v = arr.u.arr.elem[i];
            if (v.type == TOML_INT64)
                sys.cdvdman.ilink_id[i] = v.u.int64;
        }
    }

    arr = toml_get(t, "disk_id");
    if (arr.type == TOML_ARRAY && arr.u.arr.size == 5) {
        int i;
        for (i=0; i < 5; i++) {
            v = arr.u.arr.elem[i];
            if (v.type == TOML_INT64)
                sys.cdvdman.disk_id[i] = v.u.int64;
        }
    }

    return 0;
}

int load_config(toml_datum_t t)
{
    toml_datum_t arr;
    toml_datum_t v;

    // Recurse into dependencies
    arr = toml_get(t, "depends");
    if (arr.type == TOML_ARRAY) {
        int i;
        for (i=0; i < arr.u.arr.size; i++) {
            v = arr.u.arr.elem[i];
            if (v.type == TOML_STRING) {
                if (load_config_file(v.u.s, NULL) < 0) {
                    return -1;
                }
            }
        }
    }

    // Display driver set being loaded
    v = toml_get(t, "name");
    if (v.type == TOML_STRING) {
        printf("Loading: %s\n", v.u.s);
    }

    toml_string_in_overwrite(t, "default_bsd",    &sys.sBSD);
    toml_string_in_overwrite(t, "default_bsdfs",  &sys.sBSDFS);
    toml_string_in_overwrite(t, "default_dvd",    &sys.sDVDMode);
    toml_string_in_overwrite(t, "default_ata0",   &sys.sATA0File);
    toml_string_in_overwrite(t, "default_ata0id", &sys.sATA0IDFile);
    toml_string_in_overwrite(t, "default_ata1",   &sys.sATA1File);
    toml_string_in_overwrite(t, "default_mc0",    &sys.sMC0File);
    toml_string_in_overwrite(t, "default_mc1",    &sys.sMC1File);
    toml_string_in_overwrite(t, "default_elf",    &sys.sELFFile);
    toml_string_in_overwrite(t, "default_gc",     &sys.sGC);
    toml_string_in_overwrite(t, "default_gsm",    &sys.sGSM);
    toml_string_in_overwrite(t, "default_cfg",    &sys.sCFGFile);
    toml_bool_in_overwrite  (t, "default_dbc",    &sys.bDebug);
    toml_bool_in_overwrite  (t, "default_logo",   &sys.bLogo);

    load_config_eecore (toml_get(t, "eecore"));
    load_config_cdvdman(toml_get(t, "cdvdman"));

    if (modlist_add_array(&drv.mod, t) < 0)
        return -1;
    if (fakelist_add_array(&drv.fake, t) < 0)
        return -1;

    return 0;
}

toml_result_t load_config_file_toml(const char * type, const char * subtype)
{
    char filename[256];
    toml_result_t res;

    // Open and parse file
    if (subtype != NULL)
        snprintf(filename, 256, "config/%s-%s.toml", type, subtype);
    else
        snprintf(filename, 256, "config/%s.toml", type);

    res = toml_parse_file_ex(filename);
    if (!res.ok)
        printf("ERROR: failed to load and parse %s: %s\n", filename, res.errmsg);

    return res;
}

int load_config_file(const char * type, const char * subtype)
{
    toml_result_t res = load_config_file_toml(type, subtype);
    if (res.ok) {
        int rv = load_config(res.toptab);
        toml_free(res);
        return rv;
    }
    else {
        return -1;
    }
}

void *module_get_settings(struct SModule *mod)
{
    // Get a pointer to the settings data structure of a module
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

/*
 * FHI builder functions
 */
int fhi_bd_defrag_add_file_by_fd(struct fhi_bd_defrag *bdm, int fhi_fid, int fd)
{
    int i, iop_fd;
    off_t size;
    unsigned int frag_start = 0;
    struct fhi_bd_defrag_info *frag = &bdm->file[fhi_fid];

    // Get actual IOP fd
    iop_fd = ps2sdk_get_iop_fd(fd);

    // Get file size
    size = lseek64(fd, 0, SEEK_END);

    // Get current frag use count
    for (i = 0; i < FHI_MAX_FILES; i++)
        frag_start += bdm->file[i].frag_count;

    // Set fragment file
    frag->frag_start = frag_start;
    frag->frag_count = fileXioIoctl2(iop_fd, USBMASS_IOCTL_GET_FRAGLIST, NULL, 0, (void *)&bdm->frags[frag->frag_start], sizeof(bd_fragment_t) * (BDM_MAX_FRAGS - frag->frag_start));
    frag->size = size;

    // Check for max fragments
    if ((frag->frag_start + frag->frag_count) > BDM_MAX_FRAGS) {
        printf("Too many fragments (%d)\n", frag->frag_start + frag->frag_count);
        return -1;
    }

    // Debug info
    printf("file[%d] fragments: start=%u, count=%u\n", fhi_fid, frag->frag_start, frag->frag_count);
    for (i=0; i<frag->frag_count; i++)
        printf("- frag[%d] start=%u, count=%u\n", i, (unsigned int)bdm->frags[frag->frag_start+i].sector, bdm->frags[frag->frag_start+i].count);

    // Set BDM driver name and number
    // NOTE: can be set only once! Check?
    bdm->drvName = (uint32_t)fileXioIoctl2(iop_fd, USBMASS_IOCTL_GET_DRIVERNAME, NULL, 0, NULL, 0);
    fileXioIoctl2(iop_fd, USBMASS_IOCTL_GET_DEVICE_NUMBER, NULL, 0, &bdm->devNr, 4);
    char *drvName = (char *)&bdm->drvName;
    printf("Using BDM device: %s%d\n", drvName, (int)bdm->devNr);

    return 0;
}

int fhi_bd_defrag_add_file(struct fhi_bd_defrag *bdm, int fhi_fid, const char *name)
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

    rv = fhi_bd_defrag_add_file_by_fd(bdm, fhi_fid, fd);
    close(fd);
    return rv;
}

int fhi_bd_add_file_by_fd(struct fhi_bd *bdm, int fhi_fid, int fd)
{
    int iop_fd = ps2sdk_get_iop_fd(fd);

    // Set BDM driver name and number
    bdm->drvName = (uint32_t)fileXioIoctl2(iop_fd, USBMASS_IOCTL_GET_DRIVERNAME, NULL, 0, NULL, 0);
    fileXioIoctl2(iop_fd, USBMASS_IOCTL_GET_DEVICE_NUMBER, NULL, 0, &bdm->devNr, 4);
    char *drvName = (char *)&bdm->drvName;
    printf("Using BDM device: %s%d\n", drvName, (int)bdm->devNr);

    return 0;
}

int fhi_fileid_add_file_by_fd(struct fhi_fileid *ffid, int fhi_fid, int fd)
{
    int iop_fd = ps2sdk_get_iop_fd(fd);

    ffid->file[fhi_fid].id = fileXioIoctl2(iop_fd, 0x80, NULL, 0, NULL, 0);
    ffid->file[fhi_fid].size = lseek64(iop_fd, 0, SEEK_END);

    return 0;
}

int fhi_fileid_add_file(struct fhi_fileid *ffid, int fhi_fid, const char *pathname, int flags)
{
    int i, fd, rv;

    // Open file
    printf("Loading %s...\n", pathname);
    for (i = 0; i < 1000; i++) {
        fd = open(pathname, flags);
        if (fd >= 0)
            break;

        // Give low level drivers some time to init
        nopdelay();
    }
    if (fd < 0) {
        printf("Unable to open %s\n", pathname);
        return -1;
    }

    rv = fhi_fileid_add_file_by_fd(ffid, fhi_fid, fd);

    // Leave file open!
    //close(fd);

    return rv;
}

/*
 * Main neutrino loader function
 */
int main(int argc, char *argv[])
{
    irxtab_t *irxtable;
    irxptr_t *irxptr_tab;
    uint8_t *irxptr;
    int i, j;
    char sGameID[12];
    int fd_system_cnf;
    char system_cnf_data[128];
    off_t iso_size = 0;
    uint32_t layer1_lba_start = 0;

    printf("--------------------------------\n");
    printf("- Neutrino PS2 Device Emulator\n");
    printf("- Version: %s\n", GIT_TAG);
    printf("- By Maximus32\n");
    printf("--------------------------------\n");

    /*
     * Initialiaze structures before filling them from config files
     */
    memset(&sys, 0, sizeof(struct SSystemSettings));
    memset(&drv, 0, sizeof(struct SDriver));

    /*
     * Change working directory
     */
    for (i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "-cwd=", 5)) {
            if (chdir(&argv[i][5]) != 0) {
                printf("ERROR: failed to change working directory to %s\n", &argv[i][5]);
                return -1;
            }
        }
    }

    /*
     * Load system settings
     */
    if (load_config_file("system", NULL) < 0) {
        printf("ERROR: failed to load system settings\n");
        return -1;
    }

    /*
     * Debugging / testing
     * Becouse PSX2 does not support command line arguments, create a file
     * `config/pcsx2.toml` and put all arguments there. See `config/system.toml`
     * for a list of supported arguments.
     */
    //sys.sCFGFile = "pcsx2";

    /*
     * Parse user commands
     */
    const char *sDVDFile = NULL;
    const char *sATAMode = "no";
    const char *sMCMode = "no";
    int iELFArgcStart = -1;
    for (i=1; i<argc; i++) {
        //printf("argv[%d] = %s\n", i, argv[i]);
        if (!strncmp(argv[i], "-bsd=", 5))
            sys.sBSD = &argv[i][5];
        else if (!strncmp(argv[i], "-bsdfs=", 7))
            sys.sBSDFS = &argv[i][7];
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
        else if (!strncmp(argv[i], "-gc=", 4))
            sys.sGC = &argv[i][4];
        else if (!strncmp(argv[i], "-gsm=", 5))
            sys.sGSM = &argv[i][5];
        else if (!strncmp(argv[i], "-cfg=", 5))
            sys.sCFGFile = &argv[i][5];
        else if (!strncmp(argv[i], "-cwd=", 5))
            continue;
        else if (!strncmp(argv[i], "-dbc", 4))
            sys.bDebug = 1;
        else if (!strncmp(argv[i], "-logo", 5))
            sys.bLogo = 1;
        else if (!strncmp(argv[i], "-qb", 3))
            sys.bQuickBoot = 1;
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
     * Load default compat settings
     */
    toml_result_t toml_compat = load_config_file_toml("compat", NULL);
    if (!toml_compat.ok) {
        printf("ERROR: failed to load compat file\n");
        return -1;
    }
    if (load_config(toml_compat.toptab) < 0) {
        printf("ERROR: failed to load compat settings\n");
        return -1;
    }
    // Do not free the table, we need it later!

    /*
     * Load user settings
     */
    if (sys.sCFGFile != NULL) {
        if (load_config_file(sys.sCFGFile, NULL) < 0) {
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

    // Process command line game compatibility modes
    if (sys.sGC != NULL) {
        char *c;
        for (c = sys.sGC; *c != 0; c++) {
            switch (*c) {
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                {
                    const char mode[] = {'M', 'O', 'D', 'E', *c};
                    toml_datum_t t_mode = toml_get(toml_compat.toptab, mode);
                    if (t_mode.type == TOML_TABLE) {
                        if (load_config(t_mode) < 0) {
                            printf("ERROR: failed to load %s\n", mode);
                            return -1;
                        }
                    }
                    break;
                }
                default:
                    printf("ERROR: compat flag %c not supported\n", *c);
                    print_usage();
                    return -1;
            }
        }
    }

    // Process ELF file and game compatibility
    // We do this early if (while still in the LE) if possible so we can load patch files
    if (strcmp(sys.sELFFile, "auto") != 0) {
        // Not "auto", so the ELF filename should have been provided
        if ((strlen(sys.sELFFile) > 18) && (sys.sELFFile[12] == '_') && (sys.sELFFile[16] == '.')) {
            // We need the GameID
            memcpy(sGameID, &sys.sELFFile[8], 11);
            sGameID[11] = '\0';

            toml_datum_t tbl_gc = toml_get(toml_compat.toptab, sGameID);
            if (tbl_gc.type == TOML_TABLE) {
                if (load_config(tbl_gc) < 0) {
                    printf("ERROR: failed to load %s compat\n", sGameID);
                    return -1;
                }
            }
        }
        // Now free the entire compat.toml table, we no longer need it
        toml_free(toml_compat);
    }

    /*
     * Background debug colors
     */
    if (sys.bDebug)
        sys.eecore.flags |= EECORE_FLAG_DBC;

    /*
     * GSM: process user flags
     */
    if (sys.sGSM == NULL)
        goto gsm_done;

    char *pgsm = sys.sGSM;

    if (pgsm[0] == 0) {
        goto gsm_done;
    } else if (pgsm[0] != ':') {
        // Interlaced field mode
        if (!strncmp(pgsm, "fp", 2)) {
            printf("GSM: Interlaced Field Mode = Force Progressive\n");
            sys.eecore.flags |= EECORE_FLAG_GSM_FLD_FP;
            pgsm += 2;
        }

        //
        // Start: support for deprecated GSM modes
        //
        else if (!strncmp(pgsm, "1F", 2)) {
            printf("GSM: Deprecated mode 1F\n");
            sys.eecore.flags |= EECORE_FLAG_GSM_FLD_FP | EECORE_FLAG_GSM_C_1;
            goto gsm_done;
        } else if (!strncmp(pgsm, "1", 1)) {
            printf("GSM: Deprecated mode 1\n");
            sys.eecore.flags |= EECORE_FLAG_GSM_FLD_FP;
            goto gsm_done;
        } else if (!strncmp(pgsm, "2F", 2)) {
            printf("GSM: Deprecated mode 2F\n");
            sys.eecore.flags |= EECORE_FLAG_GSM_FLD_FP | EECORE_FLAG_GSM_FRM_FP2 | EECORE_FLAG_GSM_C_1;
            goto gsm_done;
        } else if (!strncmp(pgsm, "2", 1)) {
            printf("GSM: Deprecated mode 2\n");
            sys.eecore.flags |= EECORE_FLAG_GSM_FLD_FP | EECORE_FLAG_GSM_FRM_FP2;
            goto gsm_done;
        }
        //
        // End: support for deprecated GSM modes
        //

        else {
            goto gsm_error;
        }
    }

    if (pgsm[0] == 0) {
        goto gsm_done;
    } else if (pgsm[0] == ':') {
        pgsm++; // this argument
        if (pgsm[0] != ':') {
            // Interlaced frame mode
            if (!strncmp(pgsm, "fp1", 3)) {
                printf("GSM: Interlaced Frame Mode = Force Progressive 1\n");
                sys.eecore.flags |= EECORE_FLAG_GSM_FRM_FP1;
                pgsm += 3;
            } else if (!strncmp(pgsm, "fp2", 3)) {
                printf("GSM: Interlaced Frame Mode = Force Progressive 2\n");
                sys.eecore.flags |= EECORE_FLAG_GSM_FRM_FP2;
                pgsm += 3;
            } else {
                goto gsm_error;
            }
        }
    }

    if (pgsm[0] == 0) {
        goto gsm_done;
    } else if (pgsm[0] == ':') {
        pgsm++; // this argument
        //if (pgsm[0] != ':') {
            // Compatibility mode
            if (!strncmp(pgsm, "1", 1)) {
                printf("GSM: Compatibility Mode = 1\n");
                sys.eecore.flags |= EECORE_FLAG_GSM_C_1;
                pgsm += 1;
            } else if (!strncmp(pgsm, "2", 1)) {
                printf("GSM: Compatibility Mode = 2\n");
                sys.eecore.flags |= EECORE_FLAG_GSM_C_2;
                pgsm += 1;
            } else if (!strncmp(pgsm, "3", 1)) {
                printf("GSM: Compatibility Mode = 3\n");
                sys.eecore.flags |= EECORE_FLAG_GSM_C_3;
                pgsm += 1;
            } else {
                goto gsm_error;
            }
        //}
    }

    goto gsm_done;
gsm_error:
    printf("ERROR: gsm flag %s not supported\n", sys.sGSM);
    print_usage();
    return -1;
gsm_done:

    /*
     * GSM: check for 576p capability
     */
    if (sys.eecore.flags & (EECORE_FLAG_GSM_FLD_FP | EECORE_FLAG_GSM_FRM_FP1 | EECORE_FLAG_GSM_FRM_FP2)) {
        int fd_ROMVER;
        if ((fd_ROMVER = open("rom0:ROMVER", O_RDONLY)) >= 0) {
            char romver[16], romverNum[5];

            // Read ROM version
            read(fd_ROMVER, romver, sizeof(romver));
            close(fd_ROMVER);

            strncpy(romverNum, romver, 4);
            romverNum[4] = '\0';

            if (strtoul(romverNum, NULL, 10) < 210) {
                printf("WARNING: disabling GSM 576p mode on incompatible ps2 model\n");
                sys.eecore.flags |= EECORE_FLAG_GSM_NO_576P;
            }
        }
    }

    /*
     * Load backing store driver settings
     */
    if (!strcmp(sys.sBSD, "no")) {
        // Load nothing
    } else {
        if (load_config_file("bsd", sys.sBSD) < 0) {
            printf("ERROR: driver %s failed\n", sys.sBSD);
            return -1;
        }
        // mmce devices don't have a filesystem
        if (!strcmp(sys.sBSD, "mmce"))
            sys.sBSDFS = "no";
        if (!strcmp(sys.sBSDFS, "no")) {
            // Load nothing
        } else if (load_config_file("bsdfs", sys.sBSDFS) < 0) {
            printf("ERROR: driver %s failed\n", sys.sBSDFS);
            return -1;
        }
    }

    /*
     * Load CD/DVD emulation driver settings
     */
    if (!strcmp(sys.sDVDMode, "no")) {
        // Load nothing
    } else if (load_config_file("emu-dvd", sys.sDVDMode) < 0) {
        printf("ERROR: dvd driver %s failed\n", sys.sDVDMode);
        return -1;
    }

    /*
     * Load ATA emulation driver settings
     */
    if (!strcmp(sATAMode, "no")) {
        // Load nothing
    } else if (load_config_file("emu-ata", sATAMode) < 0) {
        printf("ERROR: ata driver %s failed\n", sATAMode);
        return -1;
    }

    /*
     * Load MC emulation driver settings
     */
    if (!strcmp(sMCMode, "no")) {
        // Load nothing
    } else if (load_config_file("emu-mc", sMCMode) < 0) {
        printf("ERROR: mc driver %s failed\n", sMCMode);
        return -1;
    }

    /*
     * Load all needed files before rebooting the IOP
     */
    mod_ee_core.sFileName = sys.eecore.elf;
    if (module_load(&mod_ee_core) < 0) {
        printf("ERROR: failed to load ee_core\n");
        return -1;
    }
    if (modlist_load(&drv.mod, (sys.bQuickBoot == 0) ? (MOD_ENV_LE | MOD_ENV_EE) : MOD_ENV_EE) < 0) {
        printf("ERROR: failed to load drv.mod\n");
        return -1;
    }

    /*
     * **********************************************************************
     * Changing from Boot Environment (BE) to Load Environment (LE)
     * After this point, modules and configs can no longer be accessed
     * But the BSD's will become accessible
     *
     * Except when using QuickBoot, then BE and LE are the same and all can
     * be accessed.
     * **********************************************************************
     */

    if (sys.bQuickBoot == 0) {
        /*
        * Reboot IOP into Load Environment (LE)
        */
        printf("Reboot IOP into Load Environment (LE)\n");
        //fileXioExit();
        SifExitIopHeap();
        SifLoadFileExit();
        SifExitRpc();
        SifInitRpc(0);
        while(!SifIopReset("", 0)){};
        while(!SifIopSync()) {};
        SifInitRpc(0);
        SifInitIopHeap();
        SifLoadFileInit();
        sbv_patch_enable_lmb();

        /*
        * Start load environment modules
        */
        for (i = 0; i < drv.mod.count; i++) {
            struct SModule *pm = &drv.mod.mod[i];
            if (pm->env & MOD_ENV_LE) {
                if (module_start(pm) < 0)
                    return -1;
            }
        }
        if (modlist_get_by_name(&drv.mod, "fileXio.irx") != NULL)
            fileXioInit();
    }

    // Load EE_CORE settings
    struct ee_core_data *set_ee_core = module_get_settings(&mod_ee_core);

    // FAKEMOD optional module
    // Only loaded when modules need to be faked
    struct SModule *mod_fakemod = modlist_get_by_func(&drv.mod, "FAKEMOD");

    // Load module settings for fhi_bd_defrag backing store
    struct fhi_bd_defrag *set_fhi_bd_defrag = module_get_settings(modlist_get_by_func(&drv.mod, "FHI_BD_DEFRAG"));
    if (set_fhi_bd_defrag != NULL)
        memset((void *)set_fhi_bd_defrag, 0, sizeof(struct fhi_bd_defrag));

    // Load module settings for fhi_bd backing store
    struct fhi_bd *set_fhi_bd = module_get_settings(modlist_get_by_func(&drv.mod, "FHI_BD"));
    if (set_fhi_bd != NULL)
        memset((void *)set_fhi_bd, 0, sizeof(struct fhi_bd));

    // Load module settings for fhi_fileid backing store
    struct fhi_fileid *set_fhi_fileid = module_get_settings(modlist_get_by_func(&drv.mod, "FHI_FILEID"));
    if (set_fhi_fileid != NULL)
        memset((void *)set_fhi_fileid, 0, sizeof(struct fhi_fileid));

    // Load module settings for cdvd emulator
    struct cdvdman_settings_common *set_cdvdman = module_get_settings(modlist_get_by_name(&drv.mod, "cdvdman_emu.irx"));
    if (set_cdvdman != NULL)
        memset((void *)set_cdvdman, 0, sizeof(struct cdvdman_settings_common));

    // Load module settings for module faker
    struct fakemod_data *set_fakemod = module_get_settings(modlist_get_by_name(&drv.mod, "fakemod.irx"));
    if (set_fakemod != NULL)
        memset((void *)set_fakemod, 0, sizeof(struct fakemod_data));

    // Quickboot requires certain IOP modules to be loaded before starting Neutrino
    // Re-initialize EE-side libraries using those modules
    if (sys.bQuickBoot == 1) {
        // fileXioIoctl2 needed ?
        if ((set_fhi_bd_defrag != NULL) || (set_fhi_bd != NULL) || (set_fhi_fileid != NULL)) {
            if (fileXioInit() < 0) {
                printf("ERROR: failed to initialize fileXio\n");
                return -1;
            }
        }
    }

    /*
     * Enable DVD emulation
     */
    if (sDVDFile != NULL) {
        int fd_iso = 0;

        if (set_fhi_bd_defrag == NULL && set_fhi_bd == NULL && set_fhi_fileid == NULL) {
            printf("ERROR: DVD emulator needs FHI backing store!\n");
            return -1;
        }
        if (set_cdvdman == NULL) {
            printf("ERROR: DVD emulator not found!\n");
            return -1;
        }

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
        lseek64(fd_iso, (uint64_t)layer0_lba_size * 2048, SEEK_SET);
        if (read(fd_iso, buffer, sizeof(buffer)) == sizeof(buffer)) {
            if ((buffer[0x00] == 1) && (!strncmp(&buffer[0x01], "CD001", 5))) {
                layer1_lba_start = layer0_lba_size - 16;
                printf("- DVD-DL detected\n");
            }
        }

        if (set_fhi_bd_defrag != NULL) {
            if (fhi_bd_defrag_add_file_by_fd(set_fhi_bd_defrag, FHI_FID_CDVD, fd_iso) < 0)
                return -1;
            close(fd_iso);
        } else if (set_fhi_bd != NULL) {
            if (fhi_bd_add_file_by_fd(set_fhi_bd, FHI_FID_CDVD, fd_iso) < 0)
                return -1;
            close(fd_iso);
        } else if (set_fhi_fileid != NULL) {
            const char *s = strchr(sDVDFile, ':');

            set_fhi_fileid->devNr = 0;
            if (s != NULL) {
                s--;
                if (*s >= '0' && *s <= '9')
                    set_fhi_fileid->devNr = *s - '0';
            }

            if (fhi_fileid_add_file_by_fd(set_fhi_fileid, FHI_FID_CDVD, fd_iso) < 0)
                return -1;

            // Leave file open!
            //close(fd_iso);
        }
    }

    /*
     * Figure out the the elf file to start automatically from the SYSTEM.CNF
     */
    if (strcmp(sys.sELFFile, "auto") == 0) {
        if (sDVDFile != NULL) {
            // Read SYSTEM.CNF from ISO file
            fd_system_cnf = read_system_cnf(sDVDFile, system_cnf_data, 128);
            if (fd_system_cnf < 0) {
                printf("ERROR: Unable to read SYSTEM.CNF from ISO\n");
                return -1;
            }
        } else {
            // Read SYSTEM.CNF from CD/DVD
            fd_system_cnf = open("cdrom:\\SYSTEM.CNF;1", O_RDONLY);
            if (fd_system_cnf < 0) {
                printf("ERROR: Unable to open SYSTEM.CNF from disk\n");
                return -1;
            }

            // Read file contents
            read(fd_system_cnf, system_cnf_data, 128);
            close(fd_system_cnf);
        }

        // Locate and set ELF file name
        sys.sELFFile = strstr(system_cnf_data, "cdrom0:");
        char *fname_end = strstr(system_cnf_data, ";");
        if (sys.sELFFile == NULL || fname_end == NULL) {
            printf("ERROR: file name not found in SYSTEM.CNF\n");
            return -1;
        }
        fname_end[1] = '1';
        fname_end[2] = '\0';

        if ((strlen(sys.sELFFile) > 18) && (sys.sELFFile[12] == '_') && (sys.sELFFile[16] == '.')) {
            memcpy(sGameID, &sys.sELFFile[8], 11);
            sGameID[11] = '\0';
        } else
            sGameID[0] = '\0';

        toml_datum_t tbl_gc = toml_get(toml_compat.toptab, sGameID);
        if (tbl_gc.type == TOML_TABLE) {
            if (load_config(tbl_gc) < 0) {
                printf("ERROR: failed to load %s compat\n", sGameID);
                return -1;
            }
        }
        // Now free the entire compat.toml table, we no longer need it
        toml_free(toml_compat);
    }

    /*
     * Set CDVDMAN settings
     */
    if (sDVDFile != NULL) {
        enum SCECdvdMediaType eMediaType = SCECdNODISC;
        if (sys.cdvdman.media_type != NULL) {
            if (!strncmp(sys.cdvdman.media_type, "cdda", 4)) {
                eMediaType = SCECdPS2CDDA;
            } else if (!strncmp(sys.cdvdman.media_type, "cd", 2)) {
                eMediaType = SCECdPS2CD;
            } else if (!strncmp(sys.cdvdman.media_type, "dvdv", 4)) {
                eMediaType = SCECdDVDV;
            } else if (!strncmp(sys.cdvdman.media_type, "dvd", 3)) {
                eMediaType = SCECdPS2DVD;
            }
        }
        if (eMediaType == SCECdNODISC)
            eMediaType = iso_size <= (333000 * 2048) ? SCECdPS2CD : SCECdPS2DVD;

        set_cdvdman->media = eMediaType;
        set_cdvdman->layer1_start = layer1_lba_start;
        set_cdvdman->flags = sys.cdvdman.flags;
        set_cdvdman->fs_sectors = sys.cdvdman.fs_sectors;
    } else {
        if (sys.cdvdman.flags != 0)
            printf("WARNING: compatibility cannot be changed without emulating the DVD\n");
        if (sys.cdvdman.media_type != SCECdNODISC)
            printf("WARNING: media type cannot be changed without emulating the DVD\n");
        if (sys.cdvdman.ilink_id_int != 0)
            printf("WARNING: ilink_id cannot be changed without emulating the DVD\n");
        if (sys.cdvdman.disk_id_int != 0)
            printf("WARNING: disk_id cannot be changed without emulating the DVD\n");
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
        if (set_fhi_bd_defrag != NULL) {
            if (fhi_bd_defrag_add_file(set_fhi_bd_defrag, FHI_FID_ATA0, sys.sATA0File) < 0)
                return -1;
            if (sys.sATA0IDFile != NULL) {
                if (fhi_bd_defrag_add_file(set_fhi_bd_defrag, FHI_FID_ATA0ID, sys.sATA0IDFile) < 0)
                    return -1;
            }
        } else if (set_fhi_fileid != NULL) {
            if (fhi_fileid_add_file(set_fhi_fileid, FHI_FID_ATA0, sys.sATA0File, O_RDWR) < 0)
                return -1;
            if (sys.sATA0IDFile != NULL) {
                if (fhi_fileid_add_file(set_fhi_fileid, FHI_FID_ATA0ID, sys.sATA0IDFile, O_RDONLY) < 0)
                    return -1;
            }
        } else {
            printf("ERROR: ATA emulator needs compatible FHI backing store!\n");
            return -1;
        }
    }

    /*
     * Enable ATA1 emulation
     */
    if (sys.sATA1File != NULL) {
        if (set_fhi_bd_defrag != NULL) {
            if (fhi_bd_defrag_add_file(set_fhi_bd_defrag, FHI_FID_ATA1, sys.sATA1File) < 0)
                return -1;
        } else if (set_fhi_fileid != NULL) {
            if (fhi_fileid_add_file(set_fhi_fileid, FHI_FID_ATA1, sys.sATA1File, O_RDWR) < 0)
                return -1;
        } else {
            printf("ERROR: ATA emulator needs compatible FHI backing store!\n");
            return -1;
        }
    }

    /*
     * Enable MC0 emulation
     */
    if (sys.sMC0File != NULL) {
        if (set_fhi_bd_defrag != NULL) {
            if (fhi_bd_defrag_add_file(set_fhi_bd_defrag, FHI_FID_MC0, sys.sMC0File) < 0)
                return -1;
        } else if (set_fhi_fileid != NULL) {
            if (fhi_fileid_add_file(set_fhi_fileid, FHI_FID_MC0, sys.sMC0File, O_RDWR) < 0)
                return -1;
        } else {
            printf("ERROR: MC0 emulator needs compatible FHI backing store!\n");
            return -1;
        }
    }

    /*
     * Enable MC1 emulation
     */
    if (sys.sMC1File != NULL) {
        if (set_fhi_bd_defrag != NULL) {
            if (fhi_bd_defrag_add_file(set_fhi_bd_defrag, FHI_FID_MC1, sys.sMC1File) < 0)
                return -1;
        } else if (set_fhi_fileid != NULL) {
            if (fhi_fileid_add_file(set_fhi_fileid, FHI_FID_MC1, sys.sMC1File, O_RDWR) < 0)
                return -1;
        } else {
            printf("ERROR: MC1 emulator needs compatible FHI backing store!\n");
            return -1;
        }
    }

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
    // Clear the entire "free" memory range
    memset((void *)0x00082000, 0, 0x00100000 - 0x00082000);
#pragma GCC diagnostic pop

    // Count the number of modules to pass to the ee_core
    int modcount = 4; // IOPRP, IMGDRV, UDNL and FHI
    for (i = 0; i < drv.mod.count; i++) {
        struct SModule *pm = &drv.mod.mod[i];
        if ((pm->env & MOD_ENV_EE) && (pm->sUDNL == NULL) && (pm->sFunc == NULL))
            modcount++;
    }
    if (drv.fake.count > 0) {
        // FAKEMOD
        modcount++;
    }

    irxtable = (irxtab_t *)sys.eecore.mod_base;
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
    // IMGDRV
    irxptr = module_install(modlist_get_by_func(&drv.mod, "IMGDRV"), irxptr, irxptr_tab++);
    irxtable->count++;
    // UDNL, entry is always present, even if there is no custom UDNL module
    if (modlist_get_by_func(&drv.mod, "UDNL") != NULL)
        irxptr = module_install(modlist_get_by_func(&drv.mod, "UDNL"), irxptr, irxptr_tab);
    irxptr_tab++;
    irxtable->count++;
    // FHI
    if (modlist_get_by_func(&drv.mod, "FHI_BD") != NULL) {
        // FHI BD
        irxptr = module_install(modlist_get_by_func(&drv.mod, "FHI_BD"), irxptr, irxptr_tab++);
        irxtable->count++;
    }
    else if (modlist_get_by_func(&drv.mod, "FHI_BD_DEFRAG") != NULL) {
        // FHI BD DEFRAG
        irxptr = module_install(modlist_get_by_func(&drv.mod, "FHI_BD_DEFRAG"), irxptr, irxptr_tab++);
        irxtable->count++;
    }
    else if (modlist_get_by_func(&drv.mod, "FHI_FILEID") != NULL) {
        // FHI FILEID
        irxptr = module_install(modlist_get_by_func(&drv.mod, "FHI_FILEID"), irxptr, irxptr_tab++);
        irxtable->count++;
    }
    // All other modules
    for (i = 0; i < drv.mod.count; i++) {
        struct SModule *pm = &drv.mod.mod[i];
        // Load only the modules that are not part of IOPRP and don't have a special function
        if ((pm->env & MOD_ENV_EE) && (pm->sUDNL == NULL) && (pm->sFunc == NULL)) {
            irxptr = module_install(pm, irxptr, irxptr_tab++);
            irxtable->count++;
        }
    }
    // FAKEMOD last, to prevent it from faking our own modules
    if (drv.fake.count > 0) {
        irxptr = module_install(mod_fakemod, irxptr, irxptr_tab++);
        irxtable->count++;
    }

    //
    // Set EE_CORE settings before loading into place
    //
    strncpy(set_ee_core->GameID, sGameID, 16);
    set_ee_core->CheatList       = NULL;
    set_ee_core->ModStorageStart = irxtable;
    set_ee_core->ModStorageEnd   = irxptr;
    set_ee_core->flags           = sys.eecore.flags;
    set_ee_core->iop_rm[0]       = sys.eecore.iop_reboot_mode[0];
    set_ee_core->iop_rm[1]       = sys.eecore.iop_reboot_mode[1];

    // Add simple checksum
    uint32_t *pms = (uint32_t *)irxtable;
#ifdef DEBUG
    printf("Module memory checksum:\n");
#endif
    for (j = 0; j < EEC_MOD_CHECKSUM_COUNT; j++) {
        uint32_t ssv = 0;
        for (i=0; i<1024; i++) {
            ssv += pms[i];
            // Skip imgdrv patch area
            if (pms[i] == 0xDEC1DEC1)
                i += 2;
        }
#ifdef DEBUG
        printf("- 0x%08lx = 0x%08lx\n", (uint32_t)pms, ssv);
#endif
        set_ee_core->mod_checksum_4k[j] = ssv;
        pms += 1024;
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

    // Patch PS2 to use our "ee_core.elf" instead of EELOAD
    sbvpp_replace_eeload((void *)eh->entry);
    // Patch PS2 to not wipe our module buffer
    sbvpp_patch_user_mem_clear(irxptr);

    //
    // Create EE_CORE argument string
    //
    char ee_core_arg_string[256];
    char *ee_core_argv[32];
    int ee_core_argc = 0;
    char *psConfig = ee_core_arg_string;
    size_t maxStrLen = sizeof(ee_core_arg_string);
    // PS2 Logo (optional)
    if (sys.bLogo) {
        snprintf(psConfig, maxStrLen, "%s", "rom0:PS2LOGO");
        ee_core_argv[ee_core_argc++] = psConfig;
        maxStrLen -= strlen(psConfig) + 1;
        psConfig += strlen(psConfig) + 1;
    }
    // ELF path
    snprintf(psConfig, maxStrLen, "%s", sys.sELFFile);
    ee_core_argv[ee_core_argc++] = psConfig;
    maxStrLen -= strlen(psConfig) + 1;
    psConfig += strlen(psConfig) + 1;
    // ELF args
    for (i = iELFArgcStart; i < argc; i++) {
        snprintf(psConfig, maxStrLen, "%s", argv[i]);
        ee_core_argv[ee_core_argc++] = psConfig;
        maxStrLen -= strlen(psConfig) + 1;
        psConfig += strlen(psConfig) + 1;
    }

    //
    // Print information
    //
    printf("Starting Emulation Environment\n");
    printf("------------------------------\n");
    printf("GameID = %s\n", set_ee_core->GameID);
    printf("CDVDMAN settings:\n");
    printf("- flags           = 0x%x\n",  set_cdvdman->flags);
    printf("- fs_sectors      = %d\n",    set_cdvdman->fs_sectors);
    const char *sMT;
    switch (set_cdvdman->media) {
        case SCECdPS2CDDA: sMT = "ps2 cdda";  break;
        case SCECdPS2CD:   sMT = "ps2 cd";    break;
        case SCECdDVDV:    sMT = "dvd video"; break;
        case SCECdPS2DVD:  sMT = "ps2 dvd";   break;
        default:           sMT = "unknown";
    }
    printf("- media           = %s\n",    sMT);
    printf("- layer1_start    = %d\n",    set_cdvdman->layer1_start);
    printf("- ilink_id        = %02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\n"
    , sys.cdvdman.ilink_id[0]
    , sys.cdvdman.ilink_id[1]
    , sys.cdvdman.ilink_id[2]
    , sys.cdvdman.ilink_id[3]
    , sys.cdvdman.ilink_id[4]
    , sys.cdvdman.ilink_id[5]
    , sys.cdvdman.ilink_id[6]
    , sys.cdvdman.ilink_id[7]);
    set_cdvdman->ilink_id_int = sys.cdvdman.ilink_id_int;
    printf("- disk_id         = %02x-%02x-%02x-%02x-%02x\n"
    , sys.cdvdman.disk_id[0]
    , sys.cdvdman.disk_id[1]
    , sys.cdvdman.disk_id[2]
    , sys.cdvdman.disk_id[3]
    , sys.cdvdman.disk_id[4]);
    set_cdvdman->disk_id_int = sys.cdvdman.disk_id_int;
    printf("EE CORE settings:\n");
    printf("- flags     = 0x%lx\n", set_ee_core->flags);
    printf("- iop_rm[0] = %ld\n",   set_ee_core->iop_rm[0]);
    printf("- iop_rm[1] = %ld\n",   set_ee_core->iop_rm[1]);
    printf("- mod_base  = 0x%p\n",  set_ee_core->ModStorageStart);
    printf("- args:\n");
    for (int i = 0; i < ee_core_argc; i++) {
        printf("  - [%d] %s\n", i, ee_core_argv[i]);
    }
    printf("------------------------------\n");

    /*
     * **********************************************************************
     * Changing from Load Environment (LE) to Emulation Environment (EE)
     * **********************************************************************
     */

    ExecPS2((void *)eh->entry, NULL, ee_core_argc, ee_core_argv);

    return 0;
}
