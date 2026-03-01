#ifndef MODULE_UTILS_H
#define MODULE_UTILS_H

struct SModule;
struct SModList;

struct SModule *modlist_get_by_func(struct SModList *ml, const char *func);
struct SModule *modlist_get_by_name(struct SModList *ml, const char *name);
void           *module_get_settings(struct SModule *mod);

#endif
