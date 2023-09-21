#include <string.h>

#include "ee_core_config.h"


void eecc_init(struct SEECoreConfig *eecc)
{
    // String values
    eecc_setGameMode(eecc, NULL);
    eecc_setGameID(eecc, NULL);
    eecc_setELFName(eecc, NULL);
    eecc_setELFArgs(eecc, 0, NULL);

    // Pointer values
    eecc_setKernelConfig(eecc, NULL, NULL);
    eecc_setModStorageConfig(eecc, NULL, NULL);

    // Integer values
    eecc_setCompatFlags(eecc, 0);

    // Enable bits
    eecc_setDebugColors(eecc, false);
    eecc_setPS2Logo(eecc, false);
    eecc_setPademu(eecc, false);
    eecc_setGSM(eecc, false);
    eecc_setCheats(eecc, false);

    eecc->_argc = 0;
}

//---------------------------------------------------------------------------
// String values
void eecc_setGameMode(struct SEECoreConfig *eecc, const char *gameMode)
{
    eecc->_sGameMode = gameMode;
}

void eecc_setGameID(struct SEECoreConfig *eecc, const char *gameID)
{
    eecc->_sGameID = gameID;
}

void eecc_setELFName(struct SEECoreConfig *eecc, const char *elfname)
{
    eecc->_sELFName = elfname;
}

void eecc_setELFArgs(struct SEECoreConfig *eecc, int argc, const char *argv[])
{
    eecc->_iELFArgc = argc;
    eecc->_sELFArgv = argv;
}

//---------------------------------------------------------------------------
// Pointer values
void eecc_setKernelConfig(struct SEECoreConfig *eecc, const void *eeloadCopy, const void *initUserMemory)
{
    eecc->_eeloadCopy = eeloadCopy;
    eecc->_initUserMemory = initUserMemory;
}

void eecc_setModStorageConfig(struct SEECoreConfig *eecc, const void *irxtable, const void *irxptr)
{
    eecc->_irxtable = irxtable;
    eecc->_irxptr = irxptr;
}

//---------------------------------------------------------------------------
// Integer values
void eecc_setCompatFlags(struct SEECoreConfig *eecc, unsigned int compatFlags)
{
    eecc->_compatFlags = compatFlags;
}

//---------------------------------------------------------------------------
// Enable bits
void eecc_setDebugColors(struct SEECoreConfig *eecc, bool enable)
{
    eecc->_enableDebugColors = enable;
}

void eecc_setPS2Logo(struct SEECoreConfig *eecc, bool enable)
{
    eecc->_enablePS2Logo = enable;
}

void eecc_setPademu(struct SEECoreConfig *eecc, bool enable)
{
    eecc->_enablePademu = enable;
}

void eecc_setGSM(struct SEECoreConfig *eecc, bool enable)
{
    eecc->_enableGSM = enable;
}

void eecc_setCheats(struct SEECoreConfig *eecc, bool enable)
{
    eecc->_enableCheats = enable;
}

//---------------------------------------------------------------------------
bool eecc_valid(struct SEECoreConfig *eecc)
{
    // Check for the minimum needed parameters
    if (eecc->_sELFName == NULL)
        return false;
    if (eecc->_eeloadCopy == 0x0)
        return false;
    if (eecc->_initUserMemory == 0x0)
        return false;
    if (eecc->_irxtable == 0x0)
        return false;
    if (eecc->_irxptr == 0x0)
        return false;

    return true;
}

void eecc_print(struct SEECoreConfig *eecc)
{
    eecc_argv(eecc);

    for (int i = 0; i < eecc->_argc; i++) {
        printf("[%d] %s\n", i, eecc->_argv[i]);
    }
}

int eecc_argc(struct SEECoreConfig *eecc)
{
    return eecc->_argc;
}

const char **eecc_argv(struct SEECoreConfig *eecc)
{
    int i;
    char *psConfig = eecc->_sConfig;
    size_t maxStrLen = sizeof(eecc->_sConfig);

    eecc->_argc = 0;

    // Base config
    if (eecc->_sGameMode != NULL) {
        snprintf(psConfig, maxStrLen, "-drv=%s", eecc->_sGameMode);
        eecc->_argv[eecc->_argc++] = psConfig;
        maxStrLen -= strlen(psConfig) + 1;
        psConfig += strlen(psConfig) + 1;
    }

    // Debug colors
    if (eecc->_enableDebugColors) {
        snprintf(psConfig, maxStrLen, "-v=1");
        eecc->_argv[eecc->_argc++] = psConfig;
        maxStrLen -= strlen(psConfig) + 1;
        psConfig += strlen(psConfig) + 1;
    }

    // Kernel config
    snprintf(psConfig, maxStrLen, "-kernel=%u %u", (unsigned int)eecc->_eeloadCopy, (unsigned int)eecc->_initUserMemory);
    eecc->_argv[eecc->_argc++] = psConfig;
    maxStrLen -= strlen(psConfig) + 1;
    psConfig += strlen(psConfig) + 1;

    // Module storage config
    snprintf(psConfig, maxStrLen, "-mod=%u %u", (unsigned int)eecc->_irxtable, (unsigned int)eecc->_irxptr);
    eecc->_argv[eecc->_argc++] = psConfig;
    maxStrLen -= strlen(psConfig) + 1;
    psConfig += strlen(psConfig) + 1;

    // GameID
    if (eecc->_sGameID != NULL) {
        snprintf(psConfig, maxStrLen, "-gid=%s", eecc->_sGameID);
        eecc->_argv[eecc->_argc++] = psConfig;
        maxStrLen -= strlen(psConfig) + 1;
        psConfig += strlen(psConfig) + 1;
    }

    // Compatflags
    if (eecc->_compatFlags) {
        snprintf(psConfig, maxStrLen, "-compat=%d", eecc->_compatFlags);
        eecc->_argv[eecc->_argc++] = psConfig;
        maxStrLen -= strlen(psConfig) + 1;
        psConfig += strlen(psConfig) + 1;
    }

    // BREAK! the other parameters are not for EE_CORE
    snprintf(psConfig, maxStrLen, "--b");
    eecc->_argv[eecc->_argc++] = psConfig;
    maxStrLen -= strlen(psConfig) + 1;
    psConfig += strlen(psConfig) + 1;

    // PS2 Logo (optional)
    if (eecc->_enablePS2Logo) {
        snprintf(psConfig, maxStrLen, "%s", "rom0:PS2LOGO");
        eecc->_argv[eecc->_argc++] = psConfig;
        maxStrLen -= strlen(psConfig) + 1;
        psConfig += strlen(psConfig) + 1;
    }

    // ELF path
    snprintf(psConfig, maxStrLen, "%s", eecc->_sELFName);
    eecc->_argv[eecc->_argc++] = psConfig;
    maxStrLen -= strlen(psConfig) + 1;
    psConfig += strlen(psConfig) + 1;

    // ELF args
    for (i = 0; i < eecc->_iELFArgc; i++) {
        snprintf(psConfig, maxStrLen, "%s", eecc->_sELFArgv[i]);
        eecc->_argv[eecc->_argc++] = psConfig;
        maxStrLen -= strlen(psConfig) + 1;
        psConfig += strlen(psConfig) + 1;
    }

    return eecc->_argv;
}
