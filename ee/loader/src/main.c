// libc/newlib
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
_off64_t lseek64 (int __filedes, _off64_t __offset, int __whence); // should be defined in unistd.h ???

// PS2SDK
#include <kernel.h>
#include <ps2sdkapi.h>
#include <stdint.h>
#include <loadfile.h>
#include <iopcontrol.h>
#include <libcdvd-common.h>
#include <sifrpc.h>
#include <iopheap.h>
#include <sbv_patches.h>

// Neutrino EE_CORE
#include "../../../common/include/eecore_config.h"

// Neutrino IOP modules
#include "../../../common/include/cdvdman_config.h"
#include "../../../common/include/fakemod_config.h"
#include "../../../common/include/fhi.h"

// Neutrino EE loader
#include "fhi_config.h"
#include "modlist.h"
#include "config.h"

// Neutrino
#include "elf.h"
#include "patch.h"
#include "ioprp.h"
#include "iso_cnf.h"
#include "xparam.h"
#include "tomlc17.h"

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <io_common.h>

DISABLE_PATCHED_FUNCTIONS();      // Disable the patched functionalities
DISABLE_EXTRA_TIMERS_FUNCTIONS(); // Disable the extra functionalities for timers
PS2_DISABLE_AUTOSTART_PTHREAD();  // Disable pthread functionality
void _libcglue_timezone_update() {}; // Disable timezone update
void _libcglue_rtc_update() {}; // Disable rtc update

struct SModule mod_ee_core;

void print_usage()
{
    printf("Usage: neutrino.elf options\n");
    printf("\n");
    printf("Options:\n");
    printf("  -bsd=<driver>     Backing store drivers (optional, auto-detected from path prefix), supported are:\n");
    printf("                    - no     (uses cdvd)\n");
    printf("                    - ata    (block device)\n");
    printf("                    - usb    (block device)\n");
    printf("                    - mx4sio (block device)\n");
    printf("                    - udpbd  (block device)\n");
    printf("                    - udpfs  (file system)\n");
    printf("                    - ilink  (block device)\n");
    printf("                    - mmce   (file system)\n");
    printf("\n");
    printf("  -bsdfs=<driver>   Backing store fileystem drivers used for block device, supported are:\n");
    printf("                    - exfat (default)\n");
    printf("                    - hdl   (HD Loader)\n");
    printf("                    - bd    (Block Device)\n");
    printf("                    NOTE: Used only for block devices (see -bsd)\n");
    printf("\n");
    printf("  -dvd=<mode>       DVD emulation mode, supported are:\n");
    printf("                    - no (default)\n");
    printf("                    - esr\n");
    printf("                    - <file>\n");
    printf("\n");
    printf("  -ata0=<mode>      ATA HDD 0 emulation mode, supported are:\n");
    printf("                    - no (default)\n");
    printf("                    - <file>\n");
    printf("                    NOTE: only both emulated, or both real.\n");
    printf("                          mixing not possible\n");
    printf("  -ata0id=<mode>    ATA 0 HDD ID emulation mode, supported are:\n");
    printf("                    - no (default)\n");
    printf("                    - <file>\n");
    printf("                    NOTE: only supported if ata0 is present.\n");
    printf("  -ata1=<mode>      See -ata0=<mode>\n");
    printf("\n");
    printf("  -mc0=<mode>       MC0 emulation mode, supported are:\n");
    printf("                    - no (default)\n");
    printf("                    - <file>\n");
    printf("  -mc1=<mode>       See -mc0=<mode>\n");
    printf("\n");
    printf("  -elf=<file>       ELF file to boot, supported are:\n");
    printf("                    - auto (elf file from cd/dvd) (default)\n");
    printf("                    - <file>\n");
    printf("\n");
    printf("  -gc=<compat>      Game compatibility modes, supported are:\n");
    printf("                    - 0: IOP: Fast reads (sceCdRead)\n");
    printf("                    - 1: dummy\n");
    printf("                    - 2: IOP: Sync reads (sceCdRead)\n");
    printf("                    - 3: EE : Unhook syscalls\n");
    printf("                    - 5: IOP: Emulate DVD-DL\n");
    printf("                    - 7: IOP: Fix game buffer overrun\n");
    printf("                    Multiple options possible, for example -gc=23\n");
    printf("\n");
    printf("  -gsm=v:c          GS video mode\n");
    printf("\n");
    printf("                    Parameter v = Force video mode to:\n");
    printf("                    -         : don't force (default)  (480i/576i)\n");
    printf("                    - fp1     : force 240p/288p - auto PAL/NTSC\n");
    printf("                    - fp2     : force 480p/576p - auto PAL/NTSC\n");
    printf("                    - 1080ix1 : force 1080i width x1, height x1 (very small!)\n");
    printf("                    - 1080ix2 : force 1080i width x2, height x2\n");
    printf("                    - 1080ix3 : force 1080i width x3, height x3\n");
    printf("\n");
    printf("                    Parameter c = Compatibility mode\n");
    printf("                    -      : no compatibility mode (default)\n");
    printf("                    - 1    : field flipping type 1 (GSM/OPL)\n");
    printf("                    - 2    : field flipping type 2\n");
    printf("                    - 3    : field flipping type 3\n");
    printf("\n");
    printf("                    Examples:\n");
    printf("                    -gsm=fp2      - recommended mode\n");
    printf("                    -gsm=fp2:1    - recommended mode, with compatibility 1\n");
    printf("                    -gsm=1080ix2\n");
    printf("\n");
    printf("  -cwd=<path>       Change working directory\n");
    printf("\n");
    printf("  -cfg=<file>       Load extra user/game specific config file (without .toml extension)\n");
    printf("\n");
    printf("  -dbc              Enable debug colors\n");
    printf("  -logo             Enable logo (adds rom0:PS2LOGO to arguments)\n");
    printf("  -qb               Quick-Boot directly into load environment\n");
    printf("\n");
    printf("  --b               Break, all following parameters are passed to the ELF\n");
    printf("\n");
    printf("Usage examples:\n");
    printf("  neutrino.elf -dvd=usb:path/to/filename.iso\n");
    printf("  neutrino.elf -dvd=mx4sio:path/to/filename.iso\n");
    printf("  neutrino.elf -dvd=mmce:path/to/filename.iso\n");
    printf("  neutrino.elf -dvd=ilink:path/to/filename.iso\n");
    printf("  neutrino.elf -dvd=udpbd:path/to/filename.iso\n");
    printf("  neutrino.elf -dvd=udpfs:path/to/filename.iso\n");
    printf("  neutrino.elf -dvd=ata:path/to/filename.iso\n");
    printf("  neutrino.elf -bsd=ata -bsdfs=hdl -dvd=hdl:filename.iso\n");
    printf("  neutrino.elf -bsd=udpbd -bsdfs=bd -dvd=bdfs:udp0p0\n");
}

/*
 * Parse command-line arguments into sys.* fields.
 * *out_iELFArgcStart is set to the index of the first ELF argument (after --b),
 * or argc if no --b was found.
 * Returns 0 on success, -1 on unknown argument.
 */
static int parse_cmdline_args(int argc, char *argv[], int *out_iELFArgcStart)
{
    int i;

    *out_iELFArgcStart = argc;

    for (i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "-bsd=", 5))
            sys.sBSD = &argv[i][5];
        else if (!strncmp(argv[i], "-bsdfs=", 7))
            sys.sBSDFS = &argv[i][7];
        else if (!strncmp(argv[i], "-dvd=", 5))
            sys.sDVDMode = &argv[i][5];
        else if (!strncmp(argv[i], "-ata0=", 6))
            sys.sATA0File = &argv[i][6];
        else if (!strncmp(argv[i], "-ata0id=", 8))
            sys.sATA0IDFile = &argv[i][8];
        else if (!strncmp(argv[i], "-ata1=", 6))
            sys.sATA1File = &argv[i][6];
        else if (!strncmp(argv[i], "-mc0=", 5))
            sys.sMC0File = &argv[i][5];
        else if (!strncmp(argv[i], "-mc1=", 5))
            sys.sMC1File = &argv[i][5];
        else if (!strncmp(argv[i], "-elf=", 5))
            sys.sELFFile = &argv[i][5];
        else if (!strncmp(argv[i], "-gc=", 4))
            sys.sGC = &argv[i][4];
        else if (!strncmp(argv[i], "-gsm=", 5))
            sys.sGSM = &argv[i][5];
        else if (!strncmp(argv[i], "-cfg=", 5))
            sys.sCFGFile = &argv[i][5];
        else if (!strncmp(argv[i], "-cwd=", 5))
            continue; // already handled before config loading
        else if (!strncmp(argv[i], "-dbc", 4))
            sys.bDebug = 1;
        else if (!strncmp(argv[i], "-logo", 5))
            sys.bLogo = 1;
        else if (!strncmp(argv[i], "-qb", 3))
            sys.bQuickBoot = 1;
        else if (!strncmp(argv[i], "--b", 3)) {
            *out_iELFArgcStart = i + 1;
            break;
        } else {
            printf("ERROR: unknown argv[%d] = %s\n", i, argv[i]);
            print_usage();
            return -1;
        }
    }

    return 0;
}

/*
 * Parse the -gsm= flag string stored in sys.sGSM and populate
 * sys.eecore.GsmVideoMode and sys.eecore.GsmCompMode.
 * Returns 0 on success, -1 on parse error.
 */
static int parse_gsm_flags(void)
{
    char *p = sys.sGSM;

    if (p == NULL || p[0] == 0)
        return 0;

    // Parse video mode
    if (p[0] != ':') {
        if (!strncmp(p, "fp1", 3)) {
            printf("GSM: Force 240p/288p\n");
            sys.eecore.GsmVideoMode = EECORE_GSM_VMODE_FP1;
            p += 3;
        } else if (!strncmp(p, "fp2", 3)) {
            printf("GSM: Force 480p/576p\n");
            sys.eecore.GsmVideoMode = EECORE_GSM_VMODE_FP2;
            p += 3;
        } else if (!strncmp(p, "1080ix1", 7)) {
            printf("GSM: Force 1080i x1 (width x1, height x1)\n");
            sys.eecore.GsmVideoMode = EECORE_GSM_VMODE_1080I_X1;
            p += 7;
        } else if (!strncmp(p, "1080ix2", 7)) {
            printf("GSM: Force 1080i x2 (width x2, height x2)\n");
            sys.eecore.GsmVideoMode = EECORE_GSM_VMODE_1080I_X2;
            p += 7;
        } else if (!strncmp(p, "1080ix3", 7)) {
            printf("GSM: Force 1080i x3 (width x3, height x2)\n");
            sys.eecore.GsmVideoMode = EECORE_GSM_VMODE_1080I_X3;
            p += 7;
        }
    }

    if (p[0] == 0)
        return 0;

    // Parse compatibility mode (after ':')
    if (p[0] != ':') {
        printf("ERROR: gsm flag %s not supported\n", sys.sGSM);
        print_usage();
        return -1;
    }
    p++; // skip ':'

    if (!strncmp(p, "1", 1)) {
        printf("GSM: Compatibility Mode = 1\n");
        sys.eecore.GsmCompMode = EECORE_GSM_COMP_1;
    } else if (!strncmp(p, "2", 1)) {
        printf("GSM: Compatibility Mode = 2\n");
        sys.eecore.GsmCompMode = EECORE_GSM_COMP_2;
    } else if (!strncmp(p, "3", 1)) {
        printf("GSM: Compatibility Mode = 3\n");
        sys.eecore.GsmCompMode = EECORE_GSM_COMP_3;
    } else {
        printf("ERROR: gsm flag %s not supported\n", sys.sGSM);
        print_usage();
        return -1;
    }

    return 0;
}

/*
 * Open, validate, and register a DVD ISO file with the FHI layer.
 * Detects DVD-DL by checking for a second PVD at layer0_lba_size.
 * Returns 0 on success, -1 on error.
 */
static int setup_dvd_iso(const char *path, off_t *out_iso_size,
                         uint32_t *out_layer1_lba_start)
{
    int fd;
    char buffer[6];
    uint32_t layer0_lba_size;
    int i;

    *out_iso_size        = 0;
    *out_layer1_lba_start = 0;

    printf("Loading %s...\n", path);
    for (i = 0; i < 1000; i++) {
        fd = open(path, O_RDONLY);
        if (fd >= 0)
            break;
        nopdelay();
    }
    if (fd < 0) {
        printf("Unable to open %s\n", path);
        return -1;
    }

    *out_iso_size = lseek64(fd, 0, SEEK_END);
    printf("- size = %dMiB\n", (int)(*out_iso_size / (1024 * 1024)));

    // Validate ISO header (PVD at sector 16)
    lseek64(fd, 16 * 2048, SEEK_SET);
    if (read(fd, buffer, sizeof(buffer)) != sizeof(buffer)) {
        printf("Unable to read ISO\n");
        return -1;
    }
    if ((buffer[0x00] != 1) || (strncmp(&buffer[0x01], "CD001", 5))) {
        printf("File is not a valid ISO\n");
        return -1;
    }

    // Get layer0 LBA size
    lseek64(fd, 16 * 2048 + 80, SEEK_SET);
    if (read(fd, &layer0_lba_size, sizeof(layer0_lba_size)) != sizeof(layer0_lba_size)) {
        printf("ISO invalid\n");
        return -1;
    }

    // Try to detect DVD-DL by looking for a second PVD at the layer1 boundary
    lseek64(fd, (uint64_t)layer0_lba_size * 2048, SEEK_SET);
    if (read(fd, buffer, sizeof(buffer)) == sizeof(buffer)) {
        if ((buffer[0x00] == 1) && (!strncmp(&buffer[0x01], "CD001", 5))) {
            *out_layer1_lba_start = layer0_lba_size - 16;
            printf("- DVD-DL detected\n");
        }
    }

    if (fhi_add_file_fd(FHI_FID_CDVD, fd, path) < 0)
        return -1;

    return 0;
}

/*
 * Allocate and populate the IRX module table in the EE module storage area.
 * Patches IOPRP.img and installs all EE-env modules.
 * Returns a pointer to the end of the written module data, or NULL on error.
 */
static uint8_t *build_irx_table(int dvd_active, struct SModule *mod_fakemod)
{
    irxtab_t  *irxtable;
    irxptr_t  *irxptr_tab;
    uint8_t   *irxptr;
    int        i, modcount;
    unsigned int ioprp_size;

    // Count modules: IOPRP + EE modules without sIOPRP + optional FAKEMOD
    modcount = 1; // IOPRP always present
    for (i = 0; i < drv.mod.count; i++) {
        struct SModule *pm = &drv.mod.mod[i];
        if ((pm->env & MOD_ENV_EE) && (pm->sIOPRP == NULL)) {
            printf("Module to load: %s\n", pm->sFileName);
            modcount++;
        }
    }
    if (drv.fake.count > 0)
        modcount++; // FAKEMOD

    printf("modstart %p\n", sys.eecore.ModStorageStart);
    irxtable   = (irxtab_t *)sys.eecore.ModStorageStart;
    irxptr_tab = (irxptr_t *)((unsigned char *)irxtable + sizeof(irxtab_t));
    irxptr     = (uint8_t *)((((unsigned int)irxptr_tab + sizeof(irxptr_t) * modcount) + 0xF) & ~0xF);

    irxtable->modules = irxptr_tab;
    irxtable->count   = 0;

    // Patch IOPRP.img with our custom modules (CDVDMAN, CDVDFSV, EESYNC)
    if (dvd_active)
        ioprp_size = patch_IOPRP_image((struct romdir_entry *)irxptr, ioprp_img_full.romdir, &drv.mod);
    else
        ioprp_size = patch_IOPRP_image((struct romdir_entry *)irxptr, ioprp_img_dvd.romdir, &drv.mod);
    irxptr_tab->size = ioprp_size;
    irxptr_tab->ptr  = irxptr;
    irxptr_tab++;
    irxptr += ioprp_size;
    irxtable->count++;

    // IMGDRV
    irxptr = module_install(modlist_get_by_func(&drv.mod, "IMGDRV"), irxptr, irxptr_tab++);
    irxtable->count++;

    // UDNL — entry always present, even if there is no custom UDNL module
    if (modlist_get_by_func(&drv.mod, "UDNL") != NULL)
        irxptr = module_install(modlist_get_by_func(&drv.mod, "UDNL"), irxptr, irxptr_tab);
    irxptr_tab++;
    irxtable->count++;

    // FHI BD — must be loaded before cdvdman (EE side)
    struct SModule *mod_fhi_bd = modlist_get_by_name(&drv.mod, "fhi_bd.irx");
    if (mod_fhi_bd != NULL) {
        irxptr = module_install(mod_fhi_bd, irxptr, irxptr_tab++);
        irxtable->count++;
    }

    // All other EE modules without a special function
    for (i = 0; i < drv.mod.count; i++) {
        struct SModule *pm = &drv.mod.mod[i];
        if ((pm->env & MOD_ENV_EE) && (pm->sIOPRP == NULL) && pm != mod_fhi_bd && ((pm->sFunc == NULL) || (
               strcmp(pm->sFunc, "FAKEMOD") != 0
            && strcmp(pm->sFunc, "IMGDRV") != 0
            && strcmp(pm->sFunc, "UDNL") != 0
            )))
        {
            irxptr = module_install(pm, irxptr, irxptr_tab++);
            irxtable->count++;
        }
    }

    // FAKEMOD last, to prevent it from faking our own modules
    if (drv.fake.count > 0) {
        irxptr = module_install(mod_fakemod, irxptr, irxptr_tab++);
        irxtable->count++;
    }

    return irxptr;
}

/*
 * Main neutrino loader function
 */
int main(int argc, char *argv[])
{
    int i, j;
    char sGameID[12];
    int fd_system_cnf;
    char system_cnf_data[128] = {0};
    char romver[16];
    off_t iso_size = 0;
    uint32_t layer1_lba_start = 0;
    int iELFArgcStart;

    printf("--------------------------------\n");
    printf("- Neutrino PS2 Device Emulator\n");
    printf("- Version: %s\n", GIT_TAG);
    printf("- By Maximus32\n");
    printf("--------------------------------\n");

    /*
     * Read ROMVER early for region/version checks
     */
    {
        int fd_ROMVER;
        if ((fd_ROMVER = open("rom0:ROMVER", O_RDONLY)) < 0) {
            printf("ERROR: failed to open rom0:ROMVER\n");
            return -1;
        }
        int bytes_read = read(fd_ROMVER, romver, sizeof(romver));
        close(fd_ROMVER);
        if (bytes_read < 15) {
            printf("ERROR: failed to read rom0:ROMVER\n");
            return -1;
        }
    }
    // ROMVER format: VVVVTCYYYYMMDD\n
    // [0..3]=version, [4]=region, [5]=console type, [6..13]=date YYYYMMDD
    {
        const char *region = "Unknown";
        switch (romver[4]) {
            case 'J': region = "Japan";    break;
            case 'A': region = "America";  break;
            case 'E': region = "Europe";   break;
            case 'C': region = "China";    break;
            case 'H': region = "Asia";     break;
        }
        printf("ROMVER: %.14s (v%.2s.%.2s, %s, %.4s-%.2s-%.2s, type %c)\n",
               romver,
               romver + 0, romver + 2,           // version VV.VV
               region,
               romver + 6, romver + 10, romver + 12,  // date YYYY-MM-DD
               romver[5]);                        // console type
    }

    /*
     * Initialize structures before filling them from config files
     */
    memset(&sys, 0, sizeof(struct SSystemSettings));
    memset(&drv, 0, sizeof(struct SDriver));

    /*
     * Change working directory
     */
    for (i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "-cwd=", 5)) {
            if (chdir(&argv[i][5]) != 0) {
                printf("ERROR: failed to change working directory to %s\n", &argv[i][5]);
                return -1;
            }
        }
    }

    /*
     * Detect folder layout: if config/system.toml does not exist, fall back
     * to a flat folder structure compatible with SAS (Save Application System).
     */
    {
        int fd = open("config/system.toml", O_RDONLY);
        if (fd < 0) {
            config_set_config_prefix("");
            modlist_set_modules_prefix("");
            printf("INFO: config/system.toml not found, using flat folder layout (SAS mode)\n");
        } else {
            close(fd);
        }
    }

    /*
     * Load system settings
     */
    if (load_config_file("system", NULL) < 0) {
        printf("ERROR: failed to load system settings\n");
        return -1;
    }

    /*
     * Debugging / testing
     * Becouse PSX2 does not support command line arguments, create a file
     * `config/pcsx2.toml` and put all arguments there. See `config/system.toml`
     * for a list of supported arguments.
     */
    //sys.sCFGFile = "pcsx2";

    /*
     * Parse command-line arguments
     */
    if (parse_cmdline_args(argc, argv, &iELFArgcStart) < 0)
        return -1;

    /*
     * Load default compat settings
     */
    toml_result_t toml_compat = load_config_file_toml("compat", NULL);
    if (!toml_compat.ok) {
        printf("ERROR: failed to load compat file\n");
        return -1;
    }
    if (load_config(toml_compat.toptab) < 0) {
        printf("ERROR: failed to load compat settings\n");
        return -1;
    }
    // Do not free the table, we need it later!

    /*
     * Load user settings
     */
    if (sys.sCFGFile != NULL) {
        if (load_config_file(sys.sCFGFile, NULL) < 0) {
            printf("ERROR: failed to load %s\n", sys.sCFGFile);
            return -1;
        }
    }

    // Check for "file" mode of dvd emulation
    const char *sDVDFile = NULL;
    if (strstr(sys.sDVDMode, ":")) {
        sDVDFile = sys.sDVDMode;
        sys.sDVDMode = "file";
    }

    // Check for "file" mode of ata/mc emulation
    const char *sATAMode = (sys.sATA0File != NULL || sys.sATA1File != NULL) ? "file" : "no";
    const char *sMCMode  = (sys.sMC0File  != NULL || sys.sMC1File  != NULL) ? "file" : "no";

    // Auto-detect BSD driver from path prefix when not explicitly set
    if (!strcmp(sys.sBSD, "no")) {
        const char *detected = NULL;
        if (detected == NULL) detected = bsd_from_path(sDVDFile);
        if (detected == NULL) detected = bsd_from_path(sys.sATA0File);
        if (detected == NULL) detected = bsd_from_path(sys.sATA1File);
        if (detected == NULL) detected = bsd_from_path(sys.sMC0File);
        if (detected == NULL) detected = bsd_from_path(sys.sMC1File);
        if (detected != NULL) {
            sys.sBSD = (char *)detected;
            printf("INFO: auto-detected -bsd=%s from path\n", sys.sBSD);
        }
    }

    // Process command-line game compatibility modes
    if (sys.sGC != NULL) {
        char *c;
        for (c = sys.sGC; *c != 0; c++) {
            switch (*c) {
                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                {
                    const char mode[] = {'M', 'O', 'D', 'E', *c};
                    toml_datum_t t_mode = toml_get(toml_compat.toptab, mode);
                    if (t_mode.type == TOML_TABLE) {
                        if (load_config(t_mode) < 0) {
                            printf("ERROR: failed to load %s\n", mode);
                            return -1;
                        }
                    }
                    break;
                }
                default:
                    printf("ERROR: compat flag %c not supported\n", *c);
                    print_usage();
                    return -1;
            }
        }
    }

    // Process ELF file and game compatibility early (while still in LE) so we can load patch files
    if (strcmp(sys.sELFFile, "auto") != 0) {
        // Not "auto", so the ELF filename should have been provided
        if ((strlen(sys.sELFFile) > 18) && (sys.sELFFile[12] == '_') && (sys.sELFFile[16] == '.')) {
            // Extract GameID from ELF path
            memcpy(sGameID, &sys.sELFFile[8], 11);
            sGameID[11] = '\0';

            toml_datum_t tbl_gc = toml_get(toml_compat.toptab, sGameID);
            if (tbl_gc.type == TOML_TABLE) {
                if (load_config(tbl_gc) < 0) {
                    printf("ERROR: failed to load %s compat\n", sGameID);
                    return -1;
                }
            }
        }
        // Now free the entire compat.toml table, we no longer need it
        toml_free(toml_compat);
    }

    /*
     * Background debug colors
     */
    if (sys.bDebug)
        sys.eecore.flags |= EECORE_FLAG_DBC;

    /*
     * GSM: process user flags
     */
    if (parse_gsm_flags() < 0)
        return -1;

    /*
     * GSM: check for 576p capability
     */
    if (sys.eecore.GsmVideoMode == EECORE_GSM_VMODE_FP2) {
        char romverNum[5];
        strncpy(romverNum, romver, 4);
        romverNum[4] = '\0';
        if (strtoul(romverNum, NULL, 10) < 210) {
            printf("WARNING: disabling GSM 576p mode on incompatible ps2 model\n");
            sys.eecore.flags |= EECORE_FLAG_GSM_NO_576P;
        }
    }

    /*
     * Load backing store driver settings
     */
    if (!strcmp(sys.sBSD, "no")) {
        // Load nothing
    } else {
        if (load_config_file("bsd", sys.sBSD) < 0) {
            printf("ERROR: driver %s failed\n", sys.sBSD);
            return -1;
        }

        // mmce and udpfs devices don't have a filesystem layer
        if ((!strcmp(sys.sBSD, "mmce")) || (!strcmp(sys.sBSD, "udpfs")))
            sys.sBSDFS = "no";

        if (!strcmp(sys.sBSDFS, "no")) {
            // Load nothing
        } else if (load_config_file("bsdfs", sys.sBSDFS) < 0) {
            printf("ERROR: driver %s failed\n", sys.sBSDFS);
            return -1;
        }
    }

    /*
     * Load CD/DVD emulation driver settings
     */
    if (!strcmp(sys.sDVDMode, "no")) {
        // Load nothing
    } else if (load_config_file("emu-dvd", sys.sDVDMode) < 0) {
        printf("ERROR: dvd driver %s failed\n", sys.sDVDMode);
        return -1;
    }

    /*
     * Load ATA emulation driver settings
     */
    if (!strcmp(sATAMode, "no")) {
        // Load nothing
    } else if (load_config_file("emu-ata", sATAMode) < 0) {
        printf("ERROR: ata driver %s failed\n", sATAMode);
        return -1;
    }

    /*
     * Load MC emulation driver settings
     */
    if (!strcmp(sMCMode, "no")) {
        // Load nothing
    } else if (load_config_file("emu-mc", sMCMode) < 0) {
        printf("ERROR: mc driver %s failed\n", sMCMode);
        return -1;
    }

    /*
     * Load all needed files before rebooting the IOP
     */
    mod_ee_core.sFileName = sys.eecore_elf;
    if (module_load(&mod_ee_core) < 0) {
        printf("ERROR: failed to load ee_core\n");
        return -1;
    }
    if (modlist_load(&drv.mod, (sys.bQuickBoot == 0) ? (MOD_ENV_LE | MOD_ENV_EE) : MOD_ENV_EE) < 0) {
        printf("ERROR: failed to load drv.mod\n");
        return -1;
    }

    /*
     * **********************************************************************
     * Changing from Boot Environment (BE) to Load Environment (LE)
     * After this point, modules and configs can no longer be accessed
     * But the BSD's will become accessible
     *
     * Except when using QuickBoot, then BE and LE are the same and all can
     * be accessed.
     * **********************************************************************
     */

    if (sys.bQuickBoot == 0) {
        /*
         * Reboot IOP into Load Environment (LE)
         */
        printf("Reboot IOP into Load Environment (LE)\n");
        SifExitIopHeap();
        SifLoadFileExit();
        SifExitRpc();
        SifInitRpc(0);
        while(!SifIopReset("", 0)){};
        while(!SifIopSync()) {};
        SifInitRpc(0);
        SifInitIopHeap();
        SifLoadFileInit();
        sbv_patch_enable_lmb();

        /*
         * Start load environment modules
         */
        for (i = 0; i < drv.mod.count; i++) {
            struct SModule *pm = &drv.mod.mod[i];
            if (pm->env & MOD_ENV_LE) {
                if (module_start(pm) < 0)
                    return -1;
            }
        }
        if (modlist_get_by_name(&drv.mod, "fileXio.irx") != NULL)
            fileXioInit();
    }

    // Load EE_CORE settings
    struct ee_core_data *set_ee_core = module_get_settings(&mod_ee_core);

    // FAKEMOD optional module — only needed when modules are to be faked
    struct SModule *mod_fakemod = modlist_get_by_func(&drv.mod, "FAKEMOD");

    // Detect active FHI backing store and zero its settings struct
    int fhi_active = (fhi_config_init(&drv.mod) == 0);

    // Load module settings for cdvd emulator
    struct cdvdman_settings_common *set_cdvdman = module_get_settings(modlist_get_by_name(&drv.mod, "cdvdman_emu.irx"));
    if (set_cdvdman != NULL)
        memset((void *)set_cdvdman, 0, sizeof(struct cdvdman_settings_common));

    // Load module settings for module faker
    struct fakemod_data *set_fakemod = module_get_settings(modlist_get_by_name(&drv.mod, "fakemod.irx"));
    if (set_fakemod != NULL)
        memset((void *)set_fakemod, 0, sizeof(struct fakemod_data));

    // QuickBoot requires certain IOP modules to be loaded before starting Neutrino
    if (sys.bQuickBoot == 1) {
        if (fhi_active) {
            if (fileXioInit() < 0) {
                printf("ERROR: failed to initialize fileXio\n");
                return -1;
            }
        }
    }

    /*
     * Enable DVD emulation
     */
    if (sDVDFile != NULL) {
        if (!fhi_active) {
            printf("ERROR: DVD emulator needs FHI backing store!\n");
            return -1;
        }
        if (set_cdvdman == NULL) {
            printf("ERROR: DVD emulator not found!\n");
            return -1;
        }

        if (setup_dvd_iso(sDVDFile, &iso_size, &layer1_lba_start) < 0)
            return -1;
    }

    /*
     * Figure out the ELF file to start automatically from the SYSTEM.CNF
     */
    if (strcmp(sys.sELFFile, "auto") == 0) {
        if (sDVDFile != NULL) {
            // Read SYSTEM.CNF from ISO file
            fd_system_cnf = read_system_cnf(sDVDFile, system_cnf_data, 128);
            if (fd_system_cnf < 0) {
                printf("ERROR: Unable to read SYSTEM.CNF from ISO\n");
                return -1;
            }
        } else {
            // Read SYSTEM.CNF from CD/DVD
            fd_system_cnf = open("cdrom:\\SYSTEM.CNF;1", O_RDONLY);
            if (fd_system_cnf < 0) {
                printf("ERROR: Unable to open SYSTEM.CNF from disk\n");
                return -1;
            }

            read(fd_system_cnf, system_cnf_data, 128);
            close(fd_system_cnf);
        }

        // Locate and set ELF file name
        sys.sELFFile = strstr(system_cnf_data, "cdrom0:");
        char *fname_end = strstr(system_cnf_data, ";");
        if (sys.sELFFile == NULL || fname_end == NULL) {
            printf("ERROR: file name not found in SYSTEM.CNF\n");
            return -1;
        }
        fname_end[1] = '1';
        fname_end[2] = '\0';

        if ((strlen(sys.sELFFile) > 18) && (sys.sELFFile[12] == '_') && (sys.sELFFile[16] == '.')) {
            memcpy(sGameID, &sys.sELFFile[8], 11);
            sGameID[11] = '\0';
        } else
            sGameID[0] = '\0';

        toml_datum_t tbl_gc = toml_get(toml_compat.toptab, sGameID);
        if (tbl_gc.type == TOML_TABLE) {
            if (load_config(tbl_gc) < 0) {
                printf("ERROR: failed to load per-game config for %s\n", sGameID);
                if (sys.bQuickBoot == 0) {
                    printf("       To enable full per-game config support, re-run with:\n");
                    printf("         -elf='%s'\n", sys.sELFFile);
                }
                return -1;
            }
        }
        // Now free the entire compat.toml table, we no longer need it
        toml_free(toml_compat);
    }

    /*
     * Set CDVDMAN settings
     */
    if (sDVDFile != NULL) {
        enum SCECdvdMediaType eMediaType = SCECdNODISC;
        if (sys.cdvdman.media_type != NULL) {
            if (!strncmp(sys.cdvdman.media_type, "cdda", 4)) {
                eMediaType = SCECdPS2CDDA;
            } else if (!strncmp(sys.cdvdman.media_type, "cd", 2)) {
                eMediaType = SCECdPS2CD;
            } else if (!strncmp(sys.cdvdman.media_type, "dvdv", 4)) {
                eMediaType = SCECdDVDV;
            } else if (!strncmp(sys.cdvdman.media_type, "dvd", 3)) {
                eMediaType = SCECdPS2DVD;
            }
        }
        if (eMediaType == SCECdNODISC)
            eMediaType = iso_size <= (333000 * 2048) ? SCECdPS2CD : SCECdPS2DVD;

        set_cdvdman->media        = eMediaType;
        set_cdvdman->layer1_start = layer1_lba_start;
        set_cdvdman->flags        = sys.cdvdman.flags;
        set_cdvdman->fs_sectors   = sys.cdvdman.fs_sectors;
    } else {
        if (sys.cdvdman.flags != 0)
            printf("WARNING: compatibility cannot be changed without emulating the DVD\n");
        if (sys.cdvdman.media_type != SCECdNODISC)
            printf("WARNING: media type cannot be changed without emulating the DVD\n");
        if (sys.cdvdman.ilink_id_int != 0)
            printf("WARNING: ilink_id cannot be changed without emulating the DVD\n");
        if (sys.cdvdman.disk_id_int != 0)
            printf("WARNING: disk_id cannot be changed without emulating the DVD\n");
    }

    /*
     * Set deckard compatibility
     */
    ResetDeckardXParams();
    ApplyDeckardXParam(sGameID);

    /*
     * Enable ATA0 emulation
     */
    if (sys.sATA0File != NULL) {
        if (fhi_add_file(FHI_FID_ATA0, sys.sATA0File, O_RDWR) < 0)
            return -1;
        if (sys.sATA0IDFile != NULL) {
            if (fhi_add_file(FHI_FID_ATA0ID, sys.sATA0IDFile, O_RDONLY) < 0)
                return -1;
        }
    }

    /*
     * Enable ATA1 emulation
     */
    if (sys.sATA1File != NULL) {
        if (fhi_add_file(FHI_FID_ATA1, sys.sATA1File, O_RDWR) < 0)
            return -1;
    }

    /*
     * Enable MC0 emulation
     */
    if (sys.sMC0File != NULL) {
        if (fhi_add_file(FHI_FID_MC0, sys.sMC0File, O_RDWR) < 0)
            return -1;
    }

    /*
     * Enable MC1 emulation
     */
    if (sys.sMC1File != NULL) {
        if (fhi_add_file(FHI_FID_MC1, sys.sMC1File, O_RDWR) < 0)
            return -1;
    }

    /*
     * PS2 Logo: determine region and whether a patch is needed.
     * Must be done before copying settings so flags are included in *set_ee_core.
     */
    if (sys.bLogo) {
        int consoleKnown = (romver[4] == 'E' || romver[4] == 'J' || romver[4] == 'A' ||
                            romver[4] == 'C' || romver[4] == 'H');
        int consolePAL   = (romver[4] == 'E');

        // Determine game region from VMODE in already-loaded SYSTEM.CNF
        int gameKnown = 0, gamePAL = 0;
        char *vmode = strstr(system_cnf_data, "VMODE");
        if (vmode) {
            gamePAL   = (strstr(vmode, "PAL") != NULL);
            gameKnown = 1;
        }
        // Fall back to GameID prefix
        if (!gameKnown && sGameID[0]) {
            if (!strncmp(sGameID, "SLES", 4) || !strncmp(sGameID, "SCES", 4) ||
                !strncmp(sGameID, "SLED", 4) || !strncmp(sGameID, "SCED", 4)) {
                gamePAL = 1; gameKnown = 1;
            } else if (!strncmp(sGameID, "SLUS", 4) || !strncmp(sGameID, "SCUS", 4) ||
                       !strncmp(sGameID, "SLPS", 4) || !strncmp(sGameID, "SCPS", 4) ||
                       !strncmp(sGameID, "SCAJ", 4)) {
                gamePAL = 0; gameKnown = 1;
            }
        }

        printf("Logo: console=%s game=%s\n",
               consoleKnown ? (consolePAL ? "PAL" : "NTSC") : "unknown",
               gameKnown    ? (gamePAL    ? "PAL" : "NTSC") : "unknown");

        // Enable logo if regions match (no patch needed),
        // or if both are known and differ (patch handles mismatch).
        // Skip logo if region cannot be determined to avoid showing the wrong one.
        if (consolePAL == gamePAL || (consoleKnown && gameKnown)) {
            int patch = (consolePAL != gamePAL);
            if (patch)
                sys.eecore.flags |= EECORE_FLAG_LOGO_PATCH;
            if (gamePAL)
                sys.eecore.flags |= EECORE_FLAG_LOGO_PAL;
            printf("Logo: enabled, %s, patch=%d\n", gamePAL ? "PAL" : "NTSC", patch);
        } else {
            printf("Logo: skipped (region unknown)\n");
            sys.bLogo = 0;
        }
    }

    /*
     * Fill fake module table
     */
    if (drv.fake.count > 0) {
        size_t stringbase = 0;

        if (set_fakemod == NULL || mod_fakemod == NULL) {
            printf("ERROR: fakemod not found!\n");
            return -1;
        }

        printf("Faking modules:\n");
        for (i = 0; i < drv.fake.count; i++) {
            size_t len;

            printf("- %s, %s\n", drv.fake.fake[i].fname, drv.fake.fake[i].name);

            // Copy file name into fakemod data
            len = strlen(drv.fake.fake[i].fname) + 1;
            if ((stringbase + len) > MODULE_SETTINGS_MAX_DATA_SIZE) {
                printf("Too much fake string data\n");
                return -1;
            }
            strcpy((char *)&set_fakemod->data[stringbase], drv.fake.fake[i].fname);
            set_fakemod->fake[i].fname = (char *)(stringbase + 0x80000000);
            stringbase += len;

            // Copy module name into fakemod data
            len = strlen(drv.fake.fake[i].name) + 1;
            if ((stringbase + len) > MODULE_SETTINGS_MAX_DATA_SIZE) {
                printf("Too much fake string data\n");
                return -1;
            }
            strcpy((char *)&set_fakemod->data[stringbase], drv.fake.fake[i].name);
            set_fakemod->fake[i].name = (char *)(stringbase + 0x80000000);
            stringbase += len;

            set_fakemod->fake[i].id          = 0xdead0 + i;
            set_fakemod->fake[i].prop        = drv.fake.fake[i].prop;
            set_fakemod->fake[i].version     = drv.fake.fake[i].version;
            set_fakemod->fake[i].returnLoad  = drv.fake.fake[i].returnLoad;
            set_fakemod->fake[i].returnStart = drv.fake.fake[i].returnStart;
        }
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
    // Clear the entire "free" memory range
    memset((void *)0x00082000, 0, 0x00100000 - 0x00082000);
#pragma GCC diagnostic pop

    /*
     * Build the IRX module table in EE memory
     */
    uint8_t *irxptr_end = build_irx_table(sDVDFile != NULL, mod_fakemod);
    if (irxptr_end == NULL)
        return -1;

    //
    // Set EE_CORE settings before loading into place
    //
    irxtab_t *irxtable = (irxtab_t *)sys.eecore.ModStorageStart;
    strncpy(sys.eecore.GameID, sGameID, 12);
    sys.eecore.CheatList     = NULL;
    sys.eecore.ModStorageEnd = irxptr_end;

    // Append cheat data after IRX table and point CheatList to it
    if (sys.cheats != NULL && sys.cheats_count > 0) {
        uint32_t *cheatptr = (uint32_t *)irxptr_end;
        memcpy(cheatptr, sys.cheats, sys.cheats_count * sizeof(uint32_t));
        cheatptr[sys.cheats_count]     = 0;
        cheatptr[sys.cheats_count + 1] = 0;
        sys.eecore.CheatList = (int *)irxptr_end;
        irxptr_end = (uint8_t *)(cheatptr + sys.cheats_count + 2);
    }

    // Add simple checksum over the module data
    uint32_t *pms = (uint32_t *)irxtable;
#ifdef DEBUG
    printf("Module memory checksum:\n");
#endif
    for (j = 0; j < EEC_MOD_CHECKSUM_COUNT; j++) {
        uint32_t ssv = 0;
        for (i = 0; i < 1024; i++) {
            ssv += pms[i];
            // Skip imgdrv patch area
            if (pms[i] == 0xDEC1DEC1)
                i += 2;
        }
#ifdef DEBUG
        printf("- 0x%08lx = 0x%08lx\n", (uint32_t)pms, ssv);
#endif
        sys.eecore.mod_checksum_4k[j] = ssv;
        pms += 1024;
    }

    // Copy settings to module memory
    *set_ee_core = sys.eecore;

    //
    // Load EECORE ELF sections
    //
    uint8_t *boot_elf = (uint8_t *)mod_ee_core.pData;
    elf_header_t  *eh  = (elf_header_t *)boot_elf;
    elf_pheader_t *eph = (elf_pheader_t *)(boot_elf + eh->phoff);
    for (i = 0; i < eh->phnum; i++) {
        if (eph[i].type != ELF_PT_LOAD)
            continue;

        void *pdata = (void *)(boot_elf + eph[i].offset);
        memcpy(eph[i].vaddr, pdata, eph[i].filesz);

        if (eph[i].memsz > eph[i].filesz)
            memset((uint8_t *)eph[i].vaddr + eph[i].filesz, 0, eph[i].memsz - eph[i].filesz);
    }

    // Patch PS2 to use our "ee_core.elf" instead of EELOAD
    sbvpp_replace_eeload((void *)eh->entry);
    // Patch PS2 to not wipe our module buffer
    sbvpp_patch_user_mem_clear(irxptr_end);

    //
    // Create EE_CORE argument string
    //
    char ee_core_arg_string[256];
    char *ee_core_argv[32];
    int ee_core_argc = 0;
    char *psConfig = ee_core_arg_string;
    size_t maxStrLen = sizeof(ee_core_arg_string);
    // PS2 Logo (optional)
    if (sys.bLogo) {
        snprintf(psConfig, maxStrLen, "%s", "rom0:PS2LOGO");
        ee_core_argv[ee_core_argc++] = psConfig;
        maxStrLen -= strlen(psConfig) + 1;
        psConfig  += strlen(psConfig) + 1;
    }
    // ELF path
    snprintf(psConfig, maxStrLen, "%s", sys.sELFFile);
    ee_core_argv[ee_core_argc++] = psConfig;
    maxStrLen -= strlen(psConfig) + 1;
    psConfig  += strlen(psConfig) + 1;
    // ELF args
    for (i = iELFArgcStart; i < argc; i++) {
        snprintf(psConfig, maxStrLen, "%s", argv[i]);
        ee_core_argv[ee_core_argc++] = psConfig;
        maxStrLen -= strlen(psConfig) + 1;
        psConfig  += strlen(psConfig) + 1;
    }

    //
    // Print information
    //
    printf("Starting Emulation Environment\n");
    printf("------------------------------\n");
    printf("GameID = %s\n", set_ee_core->GameID);
    printf("CDVDMAN settings:\n");
    printf("- flags           = 0x%x\n",  set_cdvdman->flags);
    printf("- fs_sectors      = %d\n",    set_cdvdman->fs_sectors);
    const char *sMT;
    switch (set_cdvdman->media) {
        case SCECdPS2CDDA: sMT = "ps2 cdda";  break;
        case SCECdPS2CD:   sMT = "ps2 cd";    break;
        case SCECdDVDV:    sMT = "dvd video"; break;
        case SCECdPS2DVD:  sMT = "ps2 dvd";   break;
        default:           sMT = "unknown";
    }
    printf("- media           = %s\n",    sMT);
    printf("- layer1_start    = %d\n",    set_cdvdman->layer1_start);
    printf("- ilink_id        = %02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\n"
    , sys.cdvdman.ilink_id[0]
    , sys.cdvdman.ilink_id[1]
    , sys.cdvdman.ilink_id[2]
    , sys.cdvdman.ilink_id[3]
    , sys.cdvdman.ilink_id[4]
    , sys.cdvdman.ilink_id[5]
    , sys.cdvdman.ilink_id[6]
    , sys.cdvdman.ilink_id[7]);
    set_cdvdman->ilink_id_int = sys.cdvdman.ilink_id_int;
    printf("- disk_id         = %02x-%02x-%02x-%02x-%02x\n"
    , sys.cdvdman.disk_id[0]
    , sys.cdvdman.disk_id[1]
    , sys.cdvdman.disk_id[2]
    , sys.cdvdman.disk_id[3]
    , sys.cdvdman.disk_id[4]);
    set_cdvdman->disk_id_int = sys.cdvdman.disk_id_int;
    printf("EE CORE settings:\n");
    printf("- flags     = 0x%lx\n", set_ee_core->flags);
    printf("- iop_rm[0] = %ld\n",   set_ee_core->iop_rm[0]);
    printf("- iop_rm[1] = %ld\n",   set_ee_core->iop_rm[1]);
    printf("- iop_rm[2] = %ld\n",   set_ee_core->iop_rm[2]);
    printf("- mod_base  = 0x%p\n",  set_ee_core->ModStorageStart);
    printf("- args:\n");
    for (int i = 0; i < ee_core_argc; i++) {
        printf("  - [%d] %s\n", i, ee_core_argv[i]);
    }
    printf("------------------------------\n");

    /*
     * **********************************************************************
     * Changing from Load Environment (LE) to Emulation Environment (EE)
     * **********************************************************************
     */

    ExecPS2((void *)eh->entry, NULL, ee_core_argc, ee_core_argv);

    return 0;
}
