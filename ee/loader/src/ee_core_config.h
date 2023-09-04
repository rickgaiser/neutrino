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
    // String values
    const char *_sGameMode;
    const char *_sGameID;
    const char *_sELFName;
    const char **_sELFArgv;
    int _iELFArgc;

    // Integer values
    u32 _eeloadCopy;
    u32 _initUserMemory;
    u32 _irxtable;
    u32 _irxptr;
    u32 _compatFlags;

    // Enable bits
    bool _enableDebugColors;
    bool _enablePS2Logo;
    bool _enablePademu;
    bool _enableGSM;
    bool _enableCheats;

    // Constructed arguments
    char _sConfig[256];
    int _argc;
    const char *_argv[32];
};

void eecc_init(struct SEECoreConfig *eecc);

// String values
void eecc_setGameMode(struct SEECoreConfig *eecc, const char *gameMode);
void eecc_setGameID(struct SEECoreConfig *eecc, const char *gameID);
void eecc_setELFName(struct SEECoreConfig *eecc, const char *elfname);
void eecc_setELFArgs(struct SEECoreConfig *eecc, int argc, const char *argv[]);

// Integer values
void eecc_setKernelConfig(struct SEECoreConfig *eecc, u32 eeloadCopy, u32 initUserMemory);
void eecc_setModStorageConfig(struct SEECoreConfig *eecc, u32 irxtable, u32 irxptr);
void eecc_setCompatFlags(struct SEECoreConfig *eecc, u32 compatFlags);

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


#endif
