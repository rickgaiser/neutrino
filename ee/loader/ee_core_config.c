#include <string.h>

#include "ee_core_config.h"


void eecc_init(struct SEECoreConfig *eecc)
{
    eecc_setGameMode(eecc, INVALID_MODE);
    eecc_setKernelConfig(eecc, 0, 0);
    eecc_setModStorageConfig(eecc, 0, 0);
    eecc_setCompatFlags(eecc, 0);
    eecc_setFileName(eecc, "");
    eecc_setExitPath(eecc, "Browser");
    IP4_ADDR(&eecc->_ethAddr, 192, 168, 1, 10);
    IP4_ADDR(&eecc->_ethMask, 255, 255, 255, 0);
    IP4_ADDR(&eecc->_ethGateway, 192, 168, 1, 1);

    eecc_setDebugColors(eecc, false);
    eecc_setPS2Logo(eecc, false);
    eecc_setPademu(eecc, false);
    eecc_setGSM(eecc, false);
    eecc_setCheats(eecc, false);
    eecc->_argc = 0;
}

void eecc_setGameMode(struct SEECoreConfig *eecc, enum GAME_MODE mode)
{
    eecc->_mode = mode;
}

void eecc_setKernelConfig(struct SEECoreConfig *eecc, u32 eeloadCopy, u32 initUserMemory)
{
    eecc->_eeloadCopy = eeloadCopy;
    eecc->_initUserMemory = initUserMemory;
}

void eecc_setModStorageConfig(struct SEECoreConfig *eecc, u32 irxtable, u32 irxptr)
{
    eecc->_irxtable = irxtable;
    eecc->_irxptr = irxptr;
}

void eecc_setCompatFlags(struct SEECoreConfig *eecc, u32 compatFlags)
{
    eecc->_compatFlags = compatFlags;
}

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

void eecc_setFileName(struct SEECoreConfig *eecc, const char *fileName)
{
    eecc->_sFileName = fileName;
}

void eecc_setExitPath(struct SEECoreConfig *eecc, const char *path)
{
    eecc->_sExitPath = path;
}

void eecc_setHDDSpindown(struct SEECoreConfig *eecc, u32 minutes)
{
    eecc->_HDDSpindown = minutes < 20 ? minutes : 20;
}

bool eecc_valid(struct SEECoreConfig *eecc)
{
    if (eecc->_mode == INVALID_MODE)
        return false;

    if (eecc->_eeloadCopy == 0x0)
        return false;

    if (eecc->_initUserMemory == 0x0)
        return false;

    if (eecc->_irxtable == 0x0)
        return false;

    if (eecc->_irxptr == 0x0)
        return false;

    if (eecc->_sFileName[0] == 0)
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
    char *psConfig = eecc->_sConfig;
    size_t maxStrLen = sizeof(eecc->_sConfig);

    eecc->_argc = 0;

    // Base config
    snprintf(psConfig, maxStrLen, "-drv=%s", eecc_getGameModeString(eecc));
    eecc->_argv[eecc->_argc++] = psConfig;
    maxStrLen -= strlen(psConfig) + 1;
    psConfig += strlen(psConfig) + 1;

    // Debug colors
    if (eecc->_enableDebugColors) {
        snprintf(psConfig, maxStrLen, "-v=1");
        eecc->_argv[eecc->_argc++] = psConfig;
        maxStrLen -= strlen(psConfig) + 1;
        psConfig += strlen(psConfig) + 1;
    }

    // Kernel config
    snprintf(psConfig, maxStrLen, "-kernel=%u %u", eecc->_eeloadCopy, eecc->_initUserMemory);
    eecc->_argv[eecc->_argc++] = psConfig;
    maxStrLen -= strlen(psConfig) + 1;
    psConfig += strlen(psConfig) + 1;

    // Module storage config
    snprintf(psConfig, maxStrLen, "-mod=%u %u", eecc->_irxtable, eecc->_irxptr);
    eecc->_argv[eecc->_argc++] = psConfig;
    maxStrLen -= strlen(psConfig) + 1;
    psConfig += strlen(psConfig) + 1;

    // Filename
    snprintf(psConfig, maxStrLen, "-file=%s", eecc->_sFileName);
    eecc->_argv[eecc->_argc++] = psConfig;
    maxStrLen -= strlen(psConfig) + 1;
    psConfig += strlen(psConfig) + 1;

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

    // Filepath
    snprintf(psConfig, maxStrLen, "cdrom0:\\%s;1", eecc->_sFileName);
    eecc->_argv[eecc->_argc++] = psConfig;
    maxStrLen -= strlen(psConfig) + 1;
    psConfig += strlen(psConfig) + 1;

    return eecc->_argv;
}

const char *eecc_getGameModeString(struct SEECoreConfig *eecc)
{
    switch (eecc->_mode) {
        case BDM_ILK_MODE:
            return "BDM_ILK_MODE";
        case BDM_M4S_MODE:
            return "BDM_M4S_MODE";
        case BDM_USB_MODE:
            return "BDM_USB_MODE";
        case BDM_UDP_MODE:
            return "BDM_UDP_MODE";
        case BDM_ATA_MODE:
            return "BDM_ATA_MODE";
        case ETH_MODE:
            return "ETH_MODE";
        case HDD_MODE:
            return "HDD_MODE";
        default:
            return "INVALID_MODE";
    }
}