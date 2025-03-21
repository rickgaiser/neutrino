/*
 * How to use:
 * - run on PCSX2 with the DVD "Auto Modellista" inserted
 * - make clean all sim
 * - debug output will be visible over EE serial output, using PCSX2
 */

// ps2sdk
#include <kernel.h>
#include <smem.h>
#include <smod.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <loadfile.h>
#include <sifcmd.h>
#include <sifrpc.h>
#include <sbv_patches.h>
#include <libcdvd-common.h>

// libc
#include <stdio.h>
#include <time.h>

// This function is defined as weak in ps2sdkc, so how
// we are not using time zone, so we can safe some KB
void _ps2sdk_timezone_update() {}

DISABLE_PATCHED_FUNCTIONS(); // Disable the patched functionalities
// DISABLE_EXTRA_TIMERS_FUNCTIONS(); // Disable the extra functionalities for timers

extern unsigned char cdvd_irx[];
extern unsigned int size_cdvd_irx;


void list_modules()
{
    smod_mod_info_t info;
    smod_mod_info_t *curr = NULL;
    char sName[21];
    u32 txtsz_total = 0;
    u32 dtasz_total = 0;
    u32 bsssz_total = 0;

    _print("\t\ttxtst   | txtsz   | dtasz   | bsssz   | versi. | name\n");
    _print("\t\t------------------------------------------------------------\n");
    while (smod_get_next_mod(curr, &info) != 0) {
        smem_read(info.name, sName, 20);
        sName[20] = 0;
        txtsz_total += info.text_size;
        dtasz_total += info.data_size;
        bsssz_total += info.bss_size;

        _print("\t\t0x%05x | %6db | %6db | %6db | 0x%4x | %s\n", info.text_start, info.text_size, info.data_size, info.bss_size, info.version, sName);
        curr = &info;
    }

    u32 total = txtsz_total + dtasz_total + bsssz_total;
    _print("\t\t------------------------------------------------------------\n");
    _print("\t\t        | %6db | %6db | %6db | total\n", txtsz_total, dtasz_total, bsssz_total);
    _print("\t\t------------------------------------------------------------\n");
    _print("\t\tTotal: %db (%dKiB)\n", total, total / 1024);
    _print("\t\t------------------------------------------------------------\n");
}

// Read buffer
#define BUF_SECTORS (128)
#define BUF_SIZE    (BUF_SECTORS*2048)
static unsigned char buffer[BUF_SIZE] __attribute__((aligned(16)));
// Callback stack
#define CB_STACK_SIZE (16*1024)
static unsigned char cb_stack[CB_STACK_SIZE];
void test_read_callback(int reason)
{
    switch(reason) {
        case SCECdFuncRead:
            _print("%s(%d=SCECdFuncRead)\n", __FUNCTION__, reason);
            break;
        case SCECdFuncReadCDDA:
            _print("%s(%d=SCECdFuncReadCDDA)\n", __FUNCTION__, reason);
            break;
        case SCECdFuncGetToc:
            _print("%s(%d=SCECdFuncGetToc)\n", __FUNCTION__, reason);
            break;
        case SCECdFuncSeek:
            _print("%s(%d=SCECdFuncSeek)\n", __FUNCTION__, reason);
            break;
        case SCECdFuncStandby:
            _print("%s(%d=SCECdFuncStandby)\n", __FUNCTION__, reason);
            break;
        case SCECdFuncStop:
            _print("%s(%d=SCECdFuncStop)\n", __FUNCTION__, reason);
            break;
        case SCECdFuncPause:
            _print("%s(%d=SCECdFuncPause)\n", __FUNCTION__, reason);
            break;
        case SCECdFuncBreak:
            _print("%s(%d=SCECdFuncBrea)\n", __FUNCTION__, reason);
            break;
        default:
            _print("%s(%d)\n", __FUNCTION__, reason);
    }
}
void test_read(const char *filename)
{
    int ret;
    sceCdlFILE fp;
    sceCdRMode mode = {
        .trycount = 0,
        .spindlctrl = SCECdSpinNom,
        .datapattern = SCECdSecS2048,
        .pad = 0
    };

    sceCdInitEeCB(20, cb_stack, CB_STACK_SIZE);
    sceCdCallback(test_read_callback);

    ret = sceCdInit(SCECdINIT);
    //if (ret == 0) {
        _print("%s: sceCdInit = %d, err = %d\n", __FUNCTION__, ret, sceCdGetError());
    //    return;
    //}

    int dt = sceCdGetDiskType();
    _print("%s: sceCdGetDiskType() = %d\n", __FUNCTION__, dt);
    _print("%s: sceCdStatus() = %d\n", __FUNCTION__, sceCdStatus());

    ret = sceCdMmode(dt);
    //if (ret == 0) {
        _print("%s: sceCdMmode(dt) = %d, err = %d\n", __FUNCTION__, ret, sceCdGetError());
    //    return;
    //}

    ret = sceCdSearchFile(&fp, filename);
    //if (ret == 0) {
        _print("%s: sceCdSearchFile(..., %s) = %d, err = %d\n", __FUNCTION__, filename, ret, sceCdGetError());
    //    return;
    //}

    ret = sceCdRead(fp.lsn, BUF_SECTORS, buffer, &mode);
    //if (ret == 0) {
        _print("%s: sceCdRead(%d, %d, 0x%x, ...) = %d, err = %d\n", __FUNCTION__, fp.lsn, BUF_SECTORS, buffer, ret, sceCdGetError());
    //    return;
    //}
#if 0
    // Blocking
    ret = sceCdSync(0);
    if (ret != 0) {
        _print("%s: sceCdSync(0) = %d, err = %d\n", __FUNCTION__, ret, sceCdGetError());
        return;
    }
#else
    // NON-Blocking
    while(sceCdSync(1)) {
        nanosleep((const struct timespec[]){{1, 0}}, NULL);
    }
#endif
}

// IOPRP
//const char *ioprp_filename = "ioprp14.img";
//const char *ioprp_filename = "ioprp15.img";
//const char *ioprp_filename = "ioprp16.img";
//const char *ioprp_filename = "ioprp165.img";
//const char *ioprp_filename = "ioprp202.img";
//const char *ioprp_filename = "ioprp205.img";
//const char *ioprp_filename = "ioprp210.img";
//const char *ioprp_filename = "ioprp211.img";
//const char *ioprp_filename = "ioprp214.img";
//const char *ioprp_filename = "ioprp224.img";
//const char *ioprp_filename = "ioprp234.img";
//const char *ioprp_filename = "ioprp241.img";
//const char *ioprp_filename = "ioprp242.img";
//const char *ioprp_filename = "ioprp243.img";
//const char *ioprp_filename = "ioprp250.img";
//const char *ioprp_filename = "ioprp253.img";
//const char *ioprp_filename = "ioprp255.img";
//const char *ioprp_filename = "ioprp260.img";
//const char *ioprp_filename = "ioprp271.img";
//const char *ioprp_filename = "ioprp271_2.img";
//const char *ioprp_filename = "ioprp280.img";
//const char *ioprp_filename = "ioprp300.img";
//const char *ioprp_filename = "ioprp300_2.img";
const char *ioprp_filename = "ioprp310.img";

// DNAS
//const char *ioprp_filename = "dnas280.img";
//const char *ioprp_filename = "dnas300_2.img";
//const char *ioprp_filename = "dnas300.img";

int main()
{
    char ioprp_string[80];

    // BIOS reboot string
    snprintf(ioprp_string, 80, "rom0:UDNL");
    // IOPRP reboot string
    //snprintf(ioprp_string, 80, "rom0:UDNL host:ioprp\\%s", ioprp_filename);

    while (!SifIopReset(ioprp_string, 0))
        ;
    while (!SifIopSync())
        ;

    SifInitRpc(0);
    SifLoadFileInit();
    SifInitIopHeap();
    sbv_patch_enable_lmb();

    _print("\n\n\n");
    _print("\t\tEE side IOPRP tester\n");
    _print("\t\tpath = %s\n", ioprp_string);
    list_modules();

    // EE side test
    test_read("afs00.afs");
    // IOP side test
    int rv = SifExecModuleBuffer(cdvd_irx, size_cdvd_irx, 0, NULL, NULL);
    _print("SifExecModuleBuffer = %d\n", rv);
    // Freeze when done
    _print("All tests done. Freeze.\n");
    while(1){}

    return 0;
}
