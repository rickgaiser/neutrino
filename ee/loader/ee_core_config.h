#ifndef EE_CORE_CONFIG_H
#define EE_CORE_CONFIG_H


#include <tamtypes.h>
#include <tcpip.h>

#include "ee_core.h"


#define bool u8
#define false 0
#define true 1


struct SEECoreConfig
{
    enum GAME_MODE _mode;
    u32 _eeloadCopy;
    u32 _initUserMemory;
    u32 _irxtable;
    u32 _irxptr;
    u32 _compatFlags;
    u32 _HDDSpindown;
    ip4_addr_t _ethAddr;
    ip4_addr_t _ethMask;
    ip4_addr_t _ethGateway;
    const char *_sFileName;
    const char *_sExitPath;

    // Enable bits
    bool _enableDebugColors;
    bool _enablePS2Logo;
    bool _enablePademu;
    bool _enableGSM;
    bool _enableCheats;

    char _sConfig[256];
    int _argc;
    const char *_argv[8];
};

void eecc_init(struct SEECoreConfig *eecc);

void eecc_setGameMode(struct SEECoreConfig *eecc, enum GAME_MODE mode);
void eecc_setKernelConfig(struct SEECoreConfig *eecc, u32 eeloadCopy, u32 initUserMemory);
void eecc_setModStorageConfig(struct SEECoreConfig *eecc, u32 irxtable, u32 irxptr);
void eecc_setCompatFlags(struct SEECoreConfig *eecc, u32 compatFlags);
void eecc_setFileName(struct SEECoreConfig *eecc, const char *fileName);
void eecc_setExitPath(struct SEECoreConfig *eecc, const char *path);
void eecc_setHDDSpindown(struct SEECoreConfig *eecc, u32 minutes);

// Enable bits
void eecc_setDebugColors(struct SEECoreConfig *eecc, bool enable);
void eecc_setPS2Logo(struct SEECoreConfig *eecc, bool enable);
void eecc_setPademu(struct SEECoreConfig *eecc, bool enable);
void eecc_setGSM(struct SEECoreConfig *eecc, bool enable);
void eecc_setCheats(struct SEECoreConfig *eecc, bool enable);

bool eecc_valid(struct SEECoreConfig *eecc);
void eecc_print(struct SEECoreConfig *eecc);
int eecc_argc(struct SEECoreConfig *eecc);
const char **eecc_argv(struct SEECoreConfig *eecc);
const char *eecc_getGameModeString(struct SEECoreConfig *eecc);


#endif
