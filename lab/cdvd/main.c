/*
 * How to use:
 * - make clean all run
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

// libc
#include <stdio.h>

// This function is defined as weak in ps2sdkc, so how
// we are not using time zone, so we can safe some KB
void _ps2sdk_timezone_update() {}

DISABLE_PATCHED_FUNCTIONS(); // Disable the patched functionalities
// DISABLE_EXTRA_TIMERS_FUNCTIONS(); // Disable the extra functionalities for timers

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
    smod_mod_info_t info;
    smod_mod_info_t *curr = NULL;
    char sName[21];
    char ioprp_string[80];
    u32 txtsz_total = 0;
    u32 dtasz_total = 0;
    u32 bsssz_total = 0;

    // BIOS reboot string
    //snprintf(ioprp_string, 80, "rom0:UDNL");
    // IOPRP reboot string
    snprintf(ioprp_string, 80, "rom0:UDNL host:ioprp\\%s", ioprp_filename);

    while (!SifIopReset(ioprp_string, 0))
        ;
    while (!SifIopSync())
        ;

    SifInitRpc(0);
    SifLoadFileInit();
    SifInitIopHeap();

    _print("\n\n\n");
    _print("\t\tIOPRP test, path = %s\n", ioprp_string);
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
    while(1){}

    return 0;
}
