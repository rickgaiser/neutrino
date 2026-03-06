// libc/newlib
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

// PS2SDK
#include <loadfile.h>

// IOP module settings
#include "../../../common/include/fakemod_config.h"

#include "modlist.h"

#define MAX_FILENAME 128

static const char *g_modules_prefix = "modules/";

void modlist_set_modules_prefix(const char *prefix)
{
    g_modules_prefix = prefix;
}

int module_load(struct SModule *mod)
{
    char sFilePath[MAX_FILENAME];

    if (mod->pData != NULL) {
        printf("WARNING: Module already loaded: %s\n", mod->sFileName);
        return 0;
    }

    if (mod->sFileName == NULL)
        return -1;

    // Open module at default location
    snprintf(sFilePath, MAX_FILENAME, "%s%s", g_modules_prefix, mod->sFileName);
    int fd = open(sFilePath, O_RDONLY);
    if (fd < 0) {
        printf("ERROR: Unable to open %s\n", mod->sFileName);
        return -1;
    }

    // Get module size
    mod->iSize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    // Allocate memory and load module
    mod->pData = malloc(mod->iSize); // NOTE: never freed, but we don't care
    read(fd, mod->pData, mod->iSize);
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
        if (strcmp(ml->mod[i].sFileName, name) == 0)
            return &ml->mod[i];
    }

    return NULL;
}

struct SModule *modlist_get_by_udnlname(struct SModList *ml, const char *name)
{
    int i;

    for (i = 0; i < ml->count; i++) {
        struct SModule *m = &ml->mod[i];
        if (m->sIOPRP != NULL && strcmp(m->sIOPRP, name) == 0)
            return m;
    }

    return NULL;
}

struct SModule *modlist_get_by_func(struct SModList *ml, const char *func)
{
    int i;

    for (i = 0; i < ml->count; i++) {
        struct SModule *m = &ml->mod[i];
        if (m->sFunc != NULL && strcmp(m->sFunc, func) == 0)
            return m;
    }

    return NULL;
}

void *module_get_settings(struct SModule *mod)
{
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

static void print_iop_args(int arg_len, const char *args)
{
    // Multiple null-terminated strings packed together
    int args_idx = 0;
    int was_null = 1;

    if (arg_len == 0)
        return;

    printf("Module arguments (arg_len=%d):\n", arg_len);

    while (args_idx < arg_len) {
        if (args[args_idx] == 0) {
            if (was_null == 1)
                printf("- args[%d]=0\n", args_idx);
            was_null = 1;
        } else if (was_null == 1) {
            printf("- args[%d]='%s'\n", args_idx, &args[args_idx]);
            was_null = 0;
        }
        args_idx++;
    }
}

uint8_t *module_install(struct SModule *mod, uint8_t *addr, irxptr_t *irx)
{
    if (mod == NULL) {
        printf("ERROR: mod == NULL\n");
        return addr;
    }

    // Install module data
    memcpy(addr, mod->pData, mod->iSize);
    irx->size = mod->iSize;
    irx->ptr  = addr;
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
