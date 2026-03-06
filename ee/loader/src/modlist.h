#ifndef MODLIST_H
#define MODLIST_H

#include <stdint.h>
#include <sys/types.h>

// PS2 SDK
#include "../../../common/include/eecore_config.h"

// IOP module settings
#include "../../../common/include/fakemod_config.h"

#define MOD_ENV_LE (1<<0)
#define MOD_ENV_EE (1<<1)
#define DRV_MAX_MOD 20

struct SModule {
    char         *sFileName;
    char         *sIOPRP;
    char         *sFunc;
    off_t         iSize;
    void         *pData;
    int           arg_len;
    char         *args;
    unsigned int  env;
};

struct SModList {
    int            count;
    struct SModule mod[DRV_MAX_MOD];
};

struct SFakeList {
    int               count;
    struct FakeModule fake[MODULE_SETTINGS_MAX_FAKE_COUNT];
};

struct SDriver {
    struct SModList  mod;
    struct SFakeList fake;
};

// Set the prefix prepended to module filenames when loading from disk.
// Default is "modules/". Pass "" for SAS flat-folder mode.
void modlist_set_modules_prefix(const char *prefix);

int             module_load(struct SModule *mod);
int             modlist_load(struct SModList *ml, unsigned int filter);
int             module_start(struct SModule *mod);

struct SModule *modlist_get_by_name(struct SModList *ml, const char *name);
struct SModule *modlist_get_by_udnlname(struct SModList *ml, const char *name);
struct SModule *modlist_get_by_func(struct SModList *ml, const char *func);

void           *module_get_settings(struct SModule *mod);
uint8_t        *module_install(struct SModule *mod, uint8_t *addr, irxptr_t *irx);

#endif
