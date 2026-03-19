// libc/newlib
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// IOP module configs
#include "../../../common/include/cdvdman_config.h"

#include "config.h"

/*
 * Global runtime state — populated by config files and command-line args.
 */
struct SSystemSettings sys;
struct SDriver         drv;

/* Folder prefix for config files — overwritten with "" in SAS flat-folder mode */
static const char *g_config_prefix = "config/";

void config_set_config_prefix(const char *prefix)
{
    g_config_prefix = prefix;
}

/*
 * Map an iomanX path prefix (e.g. "usb0", "mx4sio0", "ata0") to a -bsd config name.
 * Strips trailing digits so both "usb0:" and "usb:" match "usb".
 * Returns NULL when the prefix is ambiguous or unknown.
 */
const char *bsd_from_path(const char *path)
{
    const char *colon;
    char prefix[16];
    int len;

    if (path == NULL)
        return NULL;
    colon = strchr(path, ':');
    if (colon == NULL)
        return NULL;

    len = colon - path;
    while (len > 0 && path[len - 1] >= '0' && path[len - 1] <= '9')
        len--;
    if (len == 0 || len >= (int)sizeof(prefix))
        return NULL;
    memcpy(prefix, path, len);
    prefix[len] = '\0';

    if (strcmp(prefix, "usb") == 0)    return "usb";
    if (strcmp(prefix, "mx4sio") == 0) return "mx4sio";
    if (strcmp(prefix, "ilink") == 0)  return "ilink";
    if (strcmp(prefix, "ata") == 0)    return "ata";
    if (strcmp(prefix, "udpbd") == 0)  return "udpbd";
    if (strcmp(prefix, "mmce") == 0)   return "mmce";
    if (strcmp(prefix, "udpfs") == 0)  return "udpfs";
    return NULL;
}

/*---------------------------------------------------------------------------
 * TOML helper functions
 *---------------------------------------------------------------------------*/

void toml_string_move(toml_datum_t v, char **dest)
{
    // Free old string if previously set
    if (*dest != NULL) {
        free(*dest);
        *dest = NULL;
    }
    // Allocate and copy new string
    *dest = malloc(strlen(v.u.s) + 1);
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

/*---------------------------------------------------------------------------
 * Module / fake-module list parsers
 *---------------------------------------------------------------------------*/

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
        if (m->sFileName) free(m->sFileName);
        if (m->sIOPRP)    free(m->sIOPRP);
        if (m->sFunc)     free(m->sFunc);
        if (m->args)      free(m->args);
        if (m->pData)     free(m->pData);
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

    toml_string_in_overwrite(t, "ioprp", &m->sIOPRP);
    toml_string_in_overwrite(t, "func",  &m->sFunc);

    arr = toml_get(t, "args");
    if (arr.type == TOML_ARRAY) {
        int i;
        m->args    = malloc(256); // NOTE: never freed, but we don't care
        m->arg_len = 0;
        for (i = 0; i < arr.u.arr.size; i++) {
            v = arr.u.arr.elem[i];
            if (v.type == TOML_STRING) {
                strcpy(&m->args[m->arg_len], v.u.s);
                m->arg_len += strlen(v.u.s) + 1; // +1 for null terminator
            }
        }
    }

    arr = toml_get(t, "env");
    if (arr.type == TOML_ARRAY) {
        int i;
        for (i = 0; i < arr.u.arr.size; i++) {
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

    for (i = 0; i < arr.u.arr.size; i++) {
        toml_datum_t elem = arr.u.arr.elem[i];
        if (elem.type != TOML_TABLE)
            return -1;
        if (modlist_add(ml, elem) < 0)
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

    for (i = 0; i < arr.u.arr.size; i++) {
        toml_datum_t elem = arr.u.arr.elem[i];
        if (elem.type != TOML_TABLE)
            return -1;
        if (fakelist_add(fl, elem) < 0)
            return -1;
    }

    return 0;
}

/*---------------------------------------------------------------------------
 * Sub-section config loaders
 *---------------------------------------------------------------------------*/

int load_config_eecore(toml_datum_t t)
{
    toml_datum_t arr;
    toml_datum_t v;

    toml_string_in_overwrite(t, "elf", &sys.eecore_elf);
    v = toml_get(t, "mod_base");
    if (v.type == TOML_INT64)
        sys.eecore.ModStorageStart = (void *)(int)v.u.int64;

    arr = toml_get(t, "irm");
    if (arr.type == TOML_ARRAY && arr.u.arr.size == 3) {
        int i;
        for (i = 0; i < arr.u.arr.size; i++) {
            v = arr.u.arr.elem[i];
            if (v.type == TOML_INT64)
                sys.eecore.iop_rm[i] = v.u.int64;
        }
    }

    arr = toml_get(t, "flags");
    if (arr.type == TOML_ARRAY) {
        int i;
        for (i = 0; i < arr.u.arr.size; i++) {
            v = arr.u.arr.elem[i];
            if (v.type == TOML_STRING) {
                if (!strcmp(v.u.s, "UNHOOK")) sys.eecore.flags |= EECORE_FLAG_UNHOOK;
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
        for (i = 0; i < arr.u.arr.size; i++) {
            v = arr.u.arr.elem[i];
            if (v.type == TOML_STRING) {
                if (!strcmp(v.u.s, "FAST_READ")) sys.cdvdman.flags |= CDVDMAN_COMPAT_FAST_READ;
                if (!strcmp(v.u.s, "SYNC_READ")) sys.cdvdman.flags |= CDVDMAN_COMPAT_SYNC_READ;
                if (!strcmp(v.u.s, "DVD_DL"))    sys.cdvdman.flags |= CDVDMAN_COMPAT_DVD_DL;
            }
        }
    }

    arr = toml_get(t, "ilink_id");
    if (arr.type == TOML_ARRAY && arr.u.arr.size == 8) {
        int i;
        for (i = 0; i < 8; i++) {
            v = arr.u.arr.elem[i];
            if (v.type == TOML_INT64)
                sys.cdvdman.ilink_id[i] = v.u.int64;
        }
    }

    arr = toml_get(t, "disk_id");
    if (arr.type == TOML_ARRAY && arr.u.arr.size == 5) {
        int i;
        for (i = 0; i < 5; i++) {
            v = arr.u.arr.elem[i];
            if (v.type == TOML_INT64)
                sys.cdvdman.disk_id[i] = v.u.int64;
        }
    }

    return 0;
}

/*---------------------------------------------------------------------------
 * Top-level config loader
 *---------------------------------------------------------------------------*/

int load_config(toml_datum_t t)
{
    toml_datum_t arr;
    toml_datum_t v;

    // Recurse into dependencies
    arr = toml_get(t, "depends");
    if (arr.type == TOML_ARRAY) {
        int i;
        for (i = 0; i < arr.u.arr.size; i++) {
            v = arr.u.arr.elem[i];
            if (v.type == TOML_STRING) {
                if (load_config_file(v.u.s, NULL) < 0)
                    return -1;
            }
        }
    }

    // Display driver set being loaded
    v = toml_get(t, "name");
    if (v.type == TOML_STRING)
        printf("Loading: %s\n", v.u.s);

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

toml_result_t load_config_file_toml(const char *type, const char *subtype)
{
    char filename[256];
    toml_result_t res;

    if (subtype != NULL)
        snprintf(filename, 256, "%s%s-%s.toml", g_config_prefix, type, subtype);
    else
        snprintf(filename, 256, "%s%s.toml", g_config_prefix, type);

    res = toml_parse_file_ex(filename);
    if (!res.ok)
        printf("ERROR: failed to load and parse %s: %s\n", filename, res.errmsg);

    return res;
}

int load_config_file(const char *type, const char *subtype)
{
    toml_result_t res = load_config_file_toml(type, subtype);
    if (res.ok) {
        int rv = load_config(res.toptab);
        toml_free(res);
        return rv;
    } else {
        return -1;
    }
}
