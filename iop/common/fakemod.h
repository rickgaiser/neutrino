#ifndef FAKEMOD_H
#define FAKEMOD_H


#include <stdint.h>


struct FakeModule
{
    const char *fname;
    const char *name;
    int id;

    uint16_t prop;
    uint16_t version;

    int16_t returnLoad;  // Return value when loading the module
    int16_t returnStart; // Return value of module start function: RESIDENT END (0), NO RESIDENT END (1) or REMOVABLE END (2)
    uint16_t fill;
} __attribute__((packed));
// Fake module properties
#define FAKE_PROP_REPLACE (1<<0) /// 'fake' module is replacement module (can be used by the game)
#define FAKE_PROP_UNLOAD  (1<<1) /// 'fake' module can be unloaded (note that re-loading is not possible!)

#define MODULE_SETTINGS_MAGIC 0xf1f2f3f4
#define MODULE_SETTINGS_MAX_DATA_SIZE 256
#define MODULE_SETTINGS_MAX_FAKE_COUNT 10
struct fakemod_data
{
    // Magic number to find
    uint32_t magic;

    // Max 10 fake modules
    struct FakeModule fake[MODULE_SETTINGS_MAX_FAKE_COUNT];

    // Strings used for fake module names
    const uint8_t data[MODULE_SETTINGS_MAX_DATA_SIZE];
} __attribute__((packed));


#endif
