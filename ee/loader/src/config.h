#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// PS2SDK
#include "../../../common/include/eecore_config.h"

// IOP module configs
#include "../../../common/include/cdvdman_config.h"

// Neutrino
#include "modlist.h"
#include "tomlc17.h"

/*
 * All runtime settings parsed from command line and TOML config files.
 */
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
            uint8_t  ilink_id[8];
            uint64_t ilink_id_int;
        };
        union {
            uint8_t  disk_id[5];
            uint64_t disk_id_int; // 8 bytes, but that's ok for compare reasons
        };
    } cdvdman;

    char *eecore_elf;
    struct ee_core_data eecore;
};

extern struct SSystemSettings sys;
extern struct SDriver drv;

// Set the prefix prepended to config filenames when loading from disk.
// Default is "config/". Pass "" for SAS flat-folder mode.
void config_set_config_prefix(const char *prefix);

// Map an iomanX path prefix (e.g. "usb0:", "ata0:") to a -bsd config name.
// Returns NULL when the prefix is ambiguous or unknown.
const char *bsd_from_path(const char *path);

// TOML helpers — read a value from a TOML table and overwrite *dest
void toml_string_move(toml_datum_t v, char **dest);
void toml_string_in_overwrite(toml_datum_t t, const char *name, char **dest);
void toml_bool_in_overwrite(toml_datum_t t, const char *name, int *dest);
void toml_int_in_overwrite(toml_datum_t t, const char *name, int *dest);

// TOML config parsers — populate drv.mod / drv.fake lists from a TOML table
int modlist_add(struct SModList *ml, toml_datum_t t);
int modlist_add_array(struct SModList *ml, toml_datum_t t);
int fakelist_add(struct SFakeList *fl, toml_datum_t t);
int fakelist_add_array(struct SFakeList *fl, toml_datum_t t);

// Config loaders — populate sys and drv from TOML data
int load_config_eecore(toml_datum_t t);
int load_config_cdvdman(toml_datum_t t);
int load_config(toml_datum_t t);

// Config file loaders — open, parse, and apply a TOML config file
toml_result_t load_config_file_toml(const char *type, const char *subtype);
int           load_config_file(const char *type, const char *subtype);

#endif
