/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include <loadcore.h>
#include <stdio.h>
#include <sysclib.h>

#include "internal.h"
#include "ioplib_util.h"
#include "ioplib.h"
#include "smsutils.h"

typedef struct ModuleStatus
{
    char name[56];
    u16 version;
    u16 flags;
    int id;
    u32 entry_addr;
    u32 gp_value;
    u32 text_addr;
    u32 text_size;
    u32 data_size;
    u32 bss_size;
    u32 lreserve[2];
} ModuleStatus_t;

// MODLOAD's exports pointers
static int (*LoadStartModule)(char *modpath, int arg_len, char *args, int *modres);
static int (*StartModule)(int id, char *modname, int arg_len, char *args, int *modres);
static int (*LoadModuleBuffer)(void *ptr);
static int (*StopModule)(int id, int arg_len, char *args, int *modres);
static int (*UnloadModule)(int id);
static int (*SearchModuleByName)(const char *modname);
static int (*ReferModuleStatus)(int mid, ModuleStatus_t *status);

// modules list to fake loading
struct FakeModule
{
    const char *fname;
    const char *name;
    int id; // ID to return to the game.
    u8 flag;
    u8 prop;
    u16 version;
    s16 returnValue; // Typical return value of module. RESIDENT END (0), NO RESIDENT END (1) or REMOVABLE END (2).
};

enum FAKE_MODULE_ID {
    FAKE_MODULE_ID_DEV9 = 0xdead0,
    FAKE_MODULE_ID_USBD,
    FAKE_MODULE_ID_SMAP,
    FAKE_MODULE_ID_ATAD,
    FAKE_MODULE_ID_CDVDSTM,
    FAKE_MODULE_ID_CDVDFSV,
};

// Fake module properties
#define PROP_REPLACE (1<<0) /// 'fake' module is replacement module (can be used by the game)
#define PROP_UNLOAD  (1<<1) /// 'fake' module can be unloaded (note that re-loading is not possible!)

static struct FakeModule modulefake_list[] = {
    {"DEV9.IRX",     "dev9",             FAKE_MODULE_ID_DEV9,    FAKE_MODULE_FLAG_DEV9,    PROP_REPLACE,                0x0208, MODULE_RESIDENT_END},
    {"USBD.IRX",     "USB_driver",       FAKE_MODULE_ID_USBD,    FAKE_MODULE_FLAG_USBD,    PROP_REPLACE,                0x0204, MODULE_REMOVABLE_END},
    {"SMAP.IRX",     "INET_SMAP_driver", FAKE_MODULE_ID_SMAP,    FAKE_MODULE_FLAG_SMAP,    0,                           0x0219, MODULE_REMOVABLE_END},
    {"ENT_SMAP.IRX", "ent_smap",         FAKE_MODULE_ID_SMAP,    FAKE_MODULE_FLAG_SMAP,    0,                           0x021f, MODULE_REMOVABLE_END},
    {"ATAD.IRX",     "atad_driver",      FAKE_MODULE_ID_ATAD,    FAKE_MODULE_FLAG_ATAD,    PROP_REPLACE,                0x0207, MODULE_RESIDENT_END},
    {"CDVDSTM.IRX",  "cdvd_st_driver",   FAKE_MODULE_ID_CDVDSTM, FAKE_MODULE_FLAG_CDVDSTM, 0,                           0x0202, MODULE_REMOVABLE_END},
    {"CDVDFSV.IRX",  "cdvd_ee_driver",   FAKE_MODULE_ID_CDVDFSV, FAKE_MODULE_FLAG_CDVDFSV, PROP_REPLACE | PROP_UNLOAD,  0x0202, MODULE_REMOVABLE_END},
    {NULL, NULL, 0, 0}};

//--------------------------------------------------------------
static struct FakeModule *checkFakemodByFile(const char *path, struct FakeModule *fakemod_list)
{
    // check if module is in the list
    while (fakemod_list->fname != NULL) {
        if (strstr(path, fakemod_list->fname)) {
            return fakemod_list;
        }
        fakemod_list++;
    }

    return NULL;
}

//--------------------------------------------------------------
static struct FakeModule *checkFakemodByName(const char *modname, struct FakeModule *fakemod_list)
{
    // check if module is in the list
    while (fakemod_list->fname != NULL) {
        if (strstr(modname, fakemod_list->name)) {
            return fakemod_list;
        }
        fakemod_list++;
    }

    return NULL;
}

//--------------------------------------------------------------
static struct FakeModule *checkFakemodById(int id, struct FakeModule *fakemod_list)
{
    // check if module is in the list
    while (fakemod_list->fname != NULL) {
        if (id == fakemod_list->id) {
            return fakemod_list;
        }
        fakemod_list++;
    }

    return NULL;
}

//--------------------------------------------------------------
static int Hook_LoadStartModule(char *modpath, int arg_len, char *args, int *modres)
{
    struct FakeModule *mod;

    DPRINTF("Hook_LoadStartModule(%s)\n", modpath);

    mod = checkFakemodByFile(modpath, modulefake_list);
    if (mod != NULL && mod->flag) {
        DPRINTF("- FAKING! id=0x%x\n", mod->id);
        *modres = mod->returnValue;
        return mod->id;
    }

    return LoadStartModule(modpath, arg_len, args, modres);
}

//--------------------------------------------------------------
static int Hook_StartModule(int id, char *modname, int arg_len, char *args, int *modres)
{
    struct FakeModule *mod;

    DPRINTF("Hook_StartModule(0x%x, %s)\n", id, modname);

    mod = checkFakemodById(id, modulefake_list);
    if (mod != NULL && mod->flag) {
        DPRINTF("- FAKING! id=0x%x\n", mod->id);
        *modres = mod->returnValue;
        return mod->id;
    }

    return StartModule(id, modname, arg_len, args, modres);
}

//--------------------------------------------------------------
static int Hook_LoadModuleBuffer(void *ptr)
{
    struct FakeModule *mod;

    DPRINTF("Hook_LoadModuleBuffer() modname = %s\n", ((char *)ptr + 0x8e));

    mod = checkFakemodByName(((char *)ptr + 0x8e), modulefake_list);
    if (mod != NULL && mod->flag) {
        DPRINTF("- FAKING! id=0x%x\n", mod->id);
        return mod->id;
    }

    return LoadModuleBuffer(ptr);
}

//--------------------------------------------------------------
static int Hook_StopModule(int id, int arg_len, char *args, int *modres)
{
    struct FakeModule *mod;

    DPRINTF("Hook_StopModule(0x%x)\n", id);

    mod = checkFakemodById(id, modulefake_list);
    if (mod != NULL && mod->flag) {
        DPRINTF("- FAKING! id=0x%x\n", mod->id);

        if ((mod->prop & PROP_UNLOAD) == 0)
            *modres = MODULE_NO_RESIDENT_END;
        else
            StopModule(SearchModuleByName(mod->name), arg_len, args, modres);

        return mod->id;
    }

    return StopModule(id, arg_len, args, modres);
}

//--------------------------------------------------------------
static int Hook_UnloadModule(int id)
{
    struct FakeModule *mod;

    DPRINTF("Hook_UnloadModule(0x%x)\n", id);

    mod = checkFakemodById(id, modulefake_list);
    if (mod != NULL && mod->flag) {
        DPRINTF("- FAKING! id=0x%x\n", mod->id);

        if ((mod->prop & PROP_UNLOAD) != 0)
            UnloadModule(SearchModuleByName(mod->name));

        return mod->id;
    }

    return UnloadModule(id);
}

//--------------------------------------------------------------
static int Hook_SearchModuleByName(char *modname)
{
    struct FakeModule *mod;

    DPRINTF("Hook_SearchModuleByName(%s)\n", modname);

    mod = checkFakemodByName(modname, modulefake_list);
    if (mod != NULL && mod->flag) {
        DPRINTF("- FAKING! id=0x%x\n", mod->id);
        return mod->id;
    }

    return SearchModuleByName(modname);
}

//--------------------------------------------------------------
static int Hook_ReferModuleStatus(int id, ModuleStatus_t *status)
{
    struct FakeModule *mod;

    DPRINTF("Hook_ReferModuleStatus(0x%x)\n", id);

    mod = checkFakemodById(id, modulefake_list);
    if (mod != NULL && mod->flag && (mod->prop & PROP_REPLACE) == 0) {
        DPRINTF("- FAKING! id=0x%x\n", mod->id);
        memset(status, 0, sizeof(ModuleStatus_t));
        strcpy(status->name, mod->name);
        status->version = mod->version;
        status->id = mod->id;
        return id;
    }

    return ReferModuleStatus(id, status);
}

//--------------------------------------------------------------
void hookMODLOAD(void)
{
    // Clear the flags of unused modules
    struct FakeModule *modlist;
    for (modlist = modulefake_list; modlist->fname != NULL; modlist++)
        modlist->flag &= cdvdman_settings.common.fakemodule_flags;

    iop_library_t * lib_modload = ioplib_getByName("modload");
    LoadStartModule  = ioplib_hookExportEntry(lib_modload,  7, Hook_LoadStartModule);
    StartModule      = ioplib_hookExportEntry(lib_modload,  8, Hook_StartModule);
    LoadModuleBuffer = ioplib_hookExportEntry(lib_modload, 10, Hook_LoadModuleBuffer);
    // check modload version
    if (lib_modload->version > 0x102) {
        ReferModuleStatus  = ioplib_hookExportEntry(lib_modload, 17, Hook_ReferModuleStatus);
        StopModule         = ioplib_hookExportEntry(lib_modload, 20, Hook_StopModule);
        UnloadModule       = ioplib_hookExportEntry(lib_modload, 21, Hook_UnloadModule);
        SearchModuleByName = ioplib_hookExportEntry(lib_modload, 22, Hook_SearchModuleByName);
    } else {
        // Change all REMOVABLE END values to RESIDENT END, if modload is old.
        for (modlist = modulefake_list; modlist->fname != NULL; modlist++) {
            if (modlist->returnValue == MODULE_REMOVABLE_END)
                modlist->returnValue = MODULE_RESIDENT_END;
        }
    }

    ioplib_relinkExports(lib_modload);
}
