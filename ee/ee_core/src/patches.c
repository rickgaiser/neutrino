/*
  Copyright 2006-2008 Polo
  Copyright 2009-2010, ifcaro, jimmikaelkael & Polo
  Copyright 2016 doctorxyz
  Licenced under Academic Free License version 3.0
  Review Open-Ps2-Loader README & LICENSE files for further details.

  Some parts of the code have been taken from Polo's HD Project and doctorxyz's GSM
*/

#include "ee_core.h"
#include "util.h"
#include "modules.h"
#include "modmgr.h"

#define ALL_MODE (0xffffffff)
#define BDM_MODE (BDM_USB_MODE|BDM_M4S_MODE) // Only USB-like slow devices?
#define ETH_MODE (BDM_UDP_MODE)
#define HDD_MODE (BDM_ATA_MODE)

// This patch needs apemodpatch.irx loaded
// Currently not supported by neutrino, fix later
//#define APEMOD_PATCH

// This patch needs f2techioppatch.irx loaded
// Currently not supported by neutrino, fix later
//#define F2TECH_PATCH

// This patch needs iremsndpatch.irx loaded
// Currently not supported by neutrino, fix later
//#define IREMSSND_PATCH

typedef struct
{
    u32 addr;
    u32 val;
    u32 check;
} game_patch_t;

typedef struct
{
    char *game;
    u32 mode;
    game_patch_t patch;
} patchlist_t;

// Keep patch codes unique!
#define PATCH_GENERIC_NIS        0xDEADBEE0
#define PATCH_GENERIC_CAPCOM     0xBABECAFE
#define PATCH_GENERIC_AC9B       0xDEADBEE1
#define PATCH_GENERIC_SLOW_READS 0xDEADBEE2
#define PATCH_VIRTUA_QUEST       0xDEADBEE3
#define PATCH_SDF_MACROSS        0x00065405
#define PATCH_SRW_IMPACT         0x0021E808
#define PATCH_RNC_UYA            0x00398498
#define PATCH_ZOMBIE_ZONE        0xEEE62525
#define PATCH_DOT_HACK           0x0D074A37
#define PATCH_SOS                0x30303030
#define PATCH_ULT_PRO_PINBALL    0xBA11BA11
#define PATCH_EUTECHNYX_WU_TID   0x0012FCC8
#define PATCH_PRO_SNOWBOARDER    0x01020199
#define PATCH_SHADOW_MAN_2       0x01020413
#define PATCH_HARVEST_MOON_AWL   0xFF025421
#define PATCH_MTV_PMR_V200_ADDR  0x001F3AB8 // MTV Pimp My Ride v2.00 patch address
#define PATCH_SRS_V200_ADDR      0x0033B744 // SRS Stree Racing Syndicate v2.00 patch address

static const patchlist_t patch_list[] = {
    {"SLES_524.58", BDM_MODE, {PATCH_GENERIC_NIS, 0x00000000, 0x00000000}},        // Disgaea Hour of Darkness PAL - disable cdvd timeout stuff
    {"SLUS_206.66", BDM_MODE, {PATCH_GENERIC_NIS, 0x00000000, 0x00000000}},        // Disgaea Hour of Darkness NTSC U - disable cdvd timeout stuff
    {"SLPS_202.51", BDM_MODE, {PATCH_GENERIC_NIS, 0x00000000, 0x00000000}},        // Makai Senki Disgaea NTSC J - disable cdvd timeout stuff
    {"SLPS_202.50", BDM_MODE, {PATCH_GENERIC_NIS, 0x00000000, 0x00000000}},        // Makai Senki Disgaea (limited edition) NTSC J - disable cdvd timeout stuff
    {"SLPS_731.03", BDM_MODE, {PATCH_GENERIC_NIS, 0x00000000, 0x00000000}},        // Makai Senki Disgaea (PlayStation2 the Best) NTSC J - disable cdvd timeout stuff
    {"SLES_529.51", BDM_MODE, {PATCH_GENERIC_NIS, 0x00000000, 0x00000000}},        // Phantom Brave PAL - disable cdvd timeout stuff
    {"SLUS_209.55", BDM_MODE, {PATCH_GENERIC_NIS, 0x00000000, 0x00000000}},        // Phantom Brave NTSC U - disable cdvd timeout stuff
    {"SLPS_203.45", BDM_MODE, {PATCH_GENERIC_NIS, 0x00000000, 0x00000000}},        // Phantom Brave NTSC J - disable cdvd timeout stuff
    {"SLPS_203.44", BDM_MODE, {PATCH_GENERIC_NIS, 0x00000000, 0x00000000}},        // Phantom Brave (limited edition) NTSC J - disable cdvd timeout stuff
    {"SLPS_731.08", BDM_MODE, {PATCH_GENERIC_NIS, 0x00000000, 0x00000000}},        // Phantom Brave: 2-shuume Hajime Mashita (PlayStation 2 the Best) NTSC J - disable cdvd timeout stuff
    {"SLUS_213.17", ALL_MODE, {PATCH_GENERIC_CAPCOM, 0x00149210, 0x00000000}},     // SFA anthology US
    {"SLES_540.85", ALL_MODE, {PATCH_GENERIC_CAPCOM, 0x00148db0, 0x00000000}},     // SFA anthology EUR
    {"SLPM_664.09", ALL_MODE, {PATCH_GENERIC_CAPCOM, 0x00149210, 0x00000000}},     // SFZ Generation JP
    {"SLPM_659.98", ALL_MODE, {PATCH_GENERIC_CAPCOM, 0x00146fd0, 0x00000000}},     // Vampire: Darkstakers collection JP
    {"SLUS_212.00", BDM_MODE, {PATCH_GENERIC_AC9B, 0x00000000, 0x00000000}},       // Armored Core Nine Breaker NTSC U - skip failing case on binding a RPC server
    {"SLES_538.19", BDM_MODE, {PATCH_GENERIC_AC9B, 0x00000000, 0x00000000}},       // Armored Core Nine Breaker PAL - skip failing case on binding a RPC server
    {"SLPS_254.08", BDM_MODE, {PATCH_GENERIC_AC9B, 0x00000000, 0x00000000}},       // Armored Core Nine Breaker NTSC J - skip failing case on binding a RPC server
    {"SLUS_210.05", ALL_MODE, {PATCH_GENERIC_SLOW_READS, 0x00110000, 0x001ac514}}, // Kingdom Hearts 2 US - [Gummi mission freezing fix (check addr is where to patch,
    {"SLES_541.14", ALL_MODE, {PATCH_GENERIC_SLOW_READS, 0x00110000, 0x001ac60c}}, // Kingdom Hearts 2 UK - val is the amount of delay cycles)]
    {"SLES_542.32", ALL_MODE, {PATCH_GENERIC_SLOW_READS, 0x00110000, 0x001ac60c}}, // Kingdom Hearts 2 FR
    {"SLES_542.33", ALL_MODE, {PATCH_GENERIC_SLOW_READS, 0x00110000, 0x001ac60c}}, // Kingdom Hearts 2 DE
    {"SLES_542.34", ALL_MODE, {PATCH_GENERIC_SLOW_READS, 0x00110000, 0x001ac60c}}, // Kingdom Hearts 2 IT
    {"SLES_542.35", ALL_MODE, {PATCH_GENERIC_SLOW_READS, 0x00110000, 0x001ac60c}}, // Kingdom Hearts 2 ES
    {"SLPM_662.33", ALL_MODE, {PATCH_GENERIC_SLOW_READS, 0x00110000, 0x001ac44c}}, // Kingdom Hearts 2 JPN
    {"SLPM_666.75", ALL_MODE, {PATCH_GENERIC_SLOW_READS, 0x00149210, 0x001adf64}}, // Kingdom Hearts 2 Final Mix JPN
    {"SLUS_212.87", ETH_MODE, {PATCH_GENERIC_SLOW_READS, 0x000c0000, 0x006cd15c}}, // Prince of Persia: The Two Thrones NTSC U - slow down cdvd reads
    {"SLUS_212.87", HDD_MODE, {PATCH_GENERIC_SLOW_READS, 0x00040000, 0x006cd15c}}, // Prince of Persia: The Two Thrones NTSC U - slow down cdvd reads
    {"SLES_537.77", ETH_MODE, {PATCH_GENERIC_SLOW_READS, 0x000c0000, 0x006cd6dc}}, // Prince of Persia: The Two Thrones PAL - slow down cdvd reads
    {"SLES_537.77", HDD_MODE, {PATCH_GENERIC_SLOW_READS, 0x00040000, 0x006cd6dc}}, // Prince of Persia: The Two Thrones PAL - slow down cdvd reads
    {"SLUS_210.22", ETH_MODE, {PATCH_GENERIC_SLOW_READS, 0x000c0000, 0x0060f42c}}, // Prince of Persia: Warrior Within NTSC U - slow down cdvd reads
    {"SLUS_210.22", HDD_MODE, {PATCH_GENERIC_SLOW_READS, 0x00040000, 0x0060f42c}}, // Prince of Persia: Warrior Within NTSC U - slow down cdvd reads
    {"SLES_528.22", ETH_MODE, {PATCH_GENERIC_SLOW_READS, 0x000c0000, 0x0060f4dc}}, // Prince of Persia: Warrior Within PAL - slow down cdvd reads
    {"SLES_528.22", HDD_MODE, {PATCH_GENERIC_SLOW_READS, 0x00040000, 0x0060f4dc}}, // Prince of Persia: Warrior Within PAL - slow down cdvd reads
    {"SLUS_214.32", ALL_MODE, {PATCH_GENERIC_SLOW_READS, 0x00080000, 0x002baf34}}, // NRA Gun Club NTSC U
    {"SLUS_209.77", ALL_MODE, {PATCH_VIRTUA_QUEST, 0x00000000, 0x00000000}},       // Virtua Quest
    {"SLPM_656.32", ALL_MODE, {PATCH_VIRTUA_QUEST, 0x00000000, 0x00000000}},       // Virtua Fighter Cyber Generation: Judgment Six No Yabou
    {"SLPM_654.05", HDD_MODE, {PATCH_SDF_MACROSS, 0x00200000, 0x00249b84}},        // Super Dimensional Fortress Macross JPN
    {"SLUS_202.30", ALL_MODE, {0x00132d14, 0x10000018, 0x0c046744}},               // Max Payne NTSC U - skip IOP reset before to exec demo elfs
    {"SLES_503.25", ALL_MODE, {0x00132ce4, 0x10000018, 0x0c046744}},               // Max Payne PAL - skip IOP reset before to exec demo elfs
    {"SLUS_204.40", ALL_MODE, {0x0021bb00, 0x03e00008, 0x27bdff90}},               // Kya: Dark Lineage NTSC U - disable game debug prints
    {"SLES_514.73", ALL_MODE, {0x0021bd10, 0x03e00008, 0x27bdff90}},               // Kya: Dark Lineage PAL - disable game debug prints
    {"SLUS_204.96", ALL_MODE, {0x00104900, 0x03e00008, 0x27bdff90}},               // V-Rally 3 NTSC U - disable game debug prints
    {"SLES_507.25", ALL_MODE, {0x00104518, 0x03e00008, 0x27bdff70}},               // V-Rally 3 PAL - disable game debug prints
    {"SLUS_201.99", ALL_MODE, {0x0012a6d0, 0x24020001, 0x0c045e0a}},               // Shaun Palmer's Pro Snowboarder NTSC U
    {"SLUS_201.99", ALL_MODE, {0x0013c55c, 0x10000012, 0x04400012}},               // Shaun Palmer's Pro Snowboarder NTSC U
    {"SLES_553.46", ALL_MODE, {0x0035414C, 0x2402FFFF, 0x0C0EE74E}},               // Rugby League 2: World Cup Edition PAL
    {"SLPS_251.03", ALL_MODE, {PATCH_SRW_IMPACT, 0x00000000, 0x00000000}},         // Super Robot Wars IMPACT Limited Edition
    {"SLPS_251.04", ALL_MODE, {PATCH_SRW_IMPACT, 0x00000000, 0x00000000}},         // Super Robot Wars IMPACT
    {"SCUS_973.53", ALL_MODE, {PATCH_RNC_UYA, 0x0084c645, 0x00000000}},            // Ratchet and Clank: Up Your Arsenal
    {"SCES_524.56", ALL_MODE, {PATCH_RNC_UYA, 0x0084c726, 0x00000000}},            // Ratchet and Clank: Up Your Arsenal
    {"SLES_533.98", ALL_MODE, {PATCH_ZOMBIE_ZONE, 0x001b2c08, 0x00000000}},        // Zombie Zone
    {"SLES_544.61", ALL_MODE, {PATCH_ZOMBIE_ZONE, 0x001b3e20, 0x00000000}},        // Zombie Hunters
    {"SLPM_625.25", ALL_MODE, {PATCH_ZOMBIE_ZONE, 0x001b1dc0, 0x00000000}},        // Simple 2000 Series Vol. 61: The Oneechanbara
    {"SLPM_626.38", ALL_MODE, {PATCH_ZOMBIE_ZONE, 0x001b355c, 0x00000000}},        // Simple 2000 Series Vol. 80: The Oneechanpuruu
    {"SLES_522.37", ALL_MODE, {PATCH_DOT_HACK, 0x00000000, 0x00000000}},           // .hack//Infection PAL
    {"SLES_524.67", ALL_MODE, {PATCH_DOT_HACK, 0x00000000, 0x00000000}},           // .hack//Mutation PAL
    {"SLES_524.69", ALL_MODE, {PATCH_DOT_HACK, 0x00000000, 0x00000000}},           // .hack//Outbreak PAL
    {"SLES_524.68", ALL_MODE, {PATCH_DOT_HACK, 0x00000000, 0x00000000}},           // .hack//Quarantine PAL
    {"SLUS_205.61", ALL_MODE, {PATCH_SOS, 0x00000001, 0x00000000}},                // Disaster Report
    {"SLES_513.01", ALL_MODE, {PATCH_SOS, 0x00000002, 0x00000000}},                // SOS: The Final Escape
    {"SLPS_251.13", ALL_MODE, {PATCH_SOS, 0x00000000, 0x00000000}},                // Zettai Zetsumei Toshi
    {"SLES_535.08", ALL_MODE, {PATCH_ULT_PRO_PINBALL, 0x00000000, 0x00000000}},    // Ultimate Pro Pinball
    {"SLUS_200.02", BDM_MODE, {0x002c7758, 0x0000182d, 0x8c436d18}},               // Ridge Racer V (NTSC-U/C) - workaround disabling (bugged?) streaming code in favour of processing all data at once, for USB devices.
    {"SCES_500.00", BDM_MODE, {0x002c9760, 0x0000182d, 0x8c43a2f8}},               // Ridge Racer V (PAL) - workaround by disabling (bugged?) streaming code in favour of processing all data at once, for USB devices.
    {"SLES_552.94", ALL_MODE, {PATCH_EUTECHNYX_WU_TID, 0x0012fcc8, 0x00000000}},   // Ferrari Challenge: Trofeo Pirelli (PAL)
    {"SLUS_217.80", ALL_MODE, {PATCH_EUTECHNYX_WU_TID, 0x0012fcb0, 0x00000000}},   // Ferrari Challenge: Trofeo Pirelli (NTSC-U/C)
    {"SLUS_205.82", ALL_MODE, {PATCH_EUTECHNYX_WU_TID, 0x0033b534, 0x00000000}},   // SRS: Street Racing Syndicate (NTSC-U/C)
    {"SLES_530.45", ALL_MODE, {PATCH_EUTECHNYX_WU_TID, 0x0033fbfc, 0x00000000}},   // SRS: Street Racing Syndicate (PAL)
    {"SLUS_214.49", ALL_MODE, {PATCH_EUTECHNYX_WU_TID, 0x00361dfc, 0x00000000}},   // The Fast and the Furious (NTSC-U/C)
    {"SLES_544.83", ALL_MODE, {PATCH_EUTECHNYX_WU_TID, 0x00363c4c, 0x00000000}},   // The Fast and the Furious (PAL)
    {"SLUS_214.38", ALL_MODE, {PATCH_EUTECHNYX_WU_TID, 0x0034c944, 0x00000000}},   // Cartoon Network Racing (NTSC-U/C)
    {"SLES_543.06", ALL_MODE, {PATCH_EUTECHNYX_WU_TID, 0x0034c8A4, 0x00000000}},   // Cartoon Network Racing (PAL)
    {"SLUS_216.28", ALL_MODE, {PATCH_EUTECHNYX_WU_TID, 0x0023cbc8, 0x00000000}},   // Hot Wheels - Beat That! (NTSC-U/C)
    {"SLES_549.71", ALL_MODE, {PATCH_EUTECHNYX_WU_TID, 0x0023d7b8, 0x00000000}},   // Hot Wheels - Beat That! (PAL)
    {"SLUS_213.57", ALL_MODE, {PATCH_EUTECHNYX_WU_TID, 0x00386b14, 0x00000000}},   // Hummer Badlands (NTSC-U/C)
    {"SLES_541.58", ALL_MODE, {PATCH_EUTECHNYX_WU_TID, 0x00388a84, 0x00000000}},   // Hummer Badlands (PAL)
    {"SLUS_211.62", ALL_MODE, {PATCH_EUTECHNYX_WU_TID, 0x00144bcc, 0x00000000}},   // Ford Mustang - The Legend Lives (NTSC-U/C)
    {"SLES_532.96", ALL_MODE, {PATCH_EUTECHNYX_WU_TID, 0x00144cc4, 0x00000000}},   // Ford Mustang - The Legend Lives (PAL)
    {"SLUS_212.76", ALL_MODE, {PATCH_EUTECHNYX_WU_TID, 0x00332814, 0x00000000}},   // Ford vs. Chevy (NTSC-U/C)
    {"SLES_536.98", ALL_MODE, {PATCH_EUTECHNYX_WU_TID, 0x00335674, 0x00000000}},   // Ford vs. Chevy  (PAL)
    {"SLUS_210.86", ALL_MODE, {PATCH_EUTECHNYX_WU_TID, 0x001462fc, 0x00000000}},   // Big Mutha Truckers 2 (NTSC-U/C)
    {"SLES_529.80", ALL_MODE, {PATCH_EUTECHNYX_WU_TID, 0x00146124, 0x00000000}},   // Big Mutha Truckers 2 - Truck Me Harder (PAL)
    {"SLES_546.32", ALL_MODE, {PATCH_EUTECHNYX_WU_TID, 0x001f60f8, 0x00000000}},   // MTV Pimp My Ride (PAL)
    {"SLES_546.07", ALL_MODE, {PATCH_EUTECHNYX_WU_TID, 0x001f37d0, 0x00000000}},   // MTV Pimp My Ride (PAL-Australia)
    {"SLUS_215.80", ALL_MODE, {PATCH_EUTECHNYX_WU_TID, 0x01f52d8, 0x00000000}},    // MTV Pimp My Ride (v1.00/default) (NTSC-U/C)
    {"SLUS_201.99", ALL_MODE, {PATCH_PRO_SNOWBOARDER, 0x00000000, 0x00000000}},    // Shaun Palmer's Pro Snowboarder (NTSC-U/C)
    {"SLES_504.00", ALL_MODE, {PATCH_PRO_SNOWBOARDER, 0x00000000, 0x00000000}},    // Shaun Palmer's Pro Snowboarder (PAL)
    {"SLES_504.01", ALL_MODE, {PATCH_PRO_SNOWBOARDER, 0x00000000, 0x00000000}},    // Shaun Palmer's Pro Snowboarder (PAL French)
    {"SLES_504.02", ALL_MODE, {PATCH_PRO_SNOWBOARDER, 0x00000000, 0x00000000}},    // Shaun Palmer's Pro Snowboarder (PAL German)
    {"SLPM_651.98", ALL_MODE, {PATCH_PRO_SNOWBOARDER, 0x00000000, 0x00000000}},    // Shaun Palmer's Pro Snowboarder (NTSC-J) - Untested
    {"SLUS_204.13", ALL_MODE, {PATCH_SHADOW_MAN_2, 0x00000001, 0x00000000}},       // Shadow Man: 2econd Coming (NTSC-U/C)
    {"SLES_504.46", ALL_MODE, {PATCH_SHADOW_MAN_2, 0x00000002, 0x00000000}},       // Shadow Man: 2econd Coming (PAL)
    {"SLES_506.08", ALL_MODE, {PATCH_SHADOW_MAN_2, 0x00000003, 0x00000000}},       // Shadow Man: 2econd Coming (PAL German)
    {"SLPS_254.21", ALL_MODE, {PATCH_HARVEST_MOON_AWL, 0x00000000, 0x00000000}},   // Harvest Moon: A Wonderful Life (NTSC-J) (First Print Edition)
    {"SLPS_254.31", ALL_MODE, {PATCH_HARVEST_MOON_AWL, 0x00000000, 0x00000000}},   // Harvest Moon: A Wonderful Life (NTSC-J)
    {"SLPS_732.22", ALL_MODE, {PATCH_HARVEST_MOON_AWL, 0x00000000, 0x00000000}},   // Harvest Moon: A Wonderful Life (NTSC-J) (PlayStation 2 The Best)
    {"SLUS_211.71", ALL_MODE, {PATCH_HARVEST_MOON_AWL, 0x00000001, 0x00000000}},   // Harvest Moon: A Wonderful Life (NTSC-U/C)
    {"SLES_534.80", ALL_MODE, {PATCH_HARVEST_MOON_AWL, 0x00000002, 0x00000000}},   // Harvest Moon: A Wonderful Life (NTSC-PAL)
    {NULL, 0, {0x00000000, 0x00000000, 0x00000000}}                                // terminator
};

#define JAL(addr)      (0x0c000000 | (((addr)&0x03ffffff) >> 2))
#define JMP(addr)      (0x08000000 | (0x3ffffff & ((addr) >> 2)))
#define FNADDR(jal)    (((jal)&0x03ffffff) << 2)
#define NIBBLE2CHAR(n) ((n) <= 9 ? '0' + (n) : 'a' + (n))

static int (*cdRead)(u32 lsn, u32 nsectors, void *buf, int *mode);
static unsigned int g_delay_cycles;
static int g_mode; // Patch may use this for anything.

// Nippon Ichi Software games generic patch to disable cdvd timeout
static void NIS_generic_patches(void)
{
    static const u32 NIScdtimeoutpattern[] = {
        0x3c010000,
        0x8c230000,
        0x24630001,
        0x3c010000,
        0xac230000,
        0x3c010000,
        0x8c230000,
        0x2861037b,
        0x14200000,
        0x00000000};
    static const u32 NIScdtimeoutpattern_mask[] = {
        0xffff0000,
        0xffff0000,
        0xffffffff,
        0xffff0000,
        0xffff0000,
        0xffff0000,
        0xffff0000,
        0xffffffff,
        0xffff0000,
        0xffffffff};
    u32 *ptr;

    ptr = find_pattern_with_mask((u32 *)0x00100000, 0x01e00000, NIScdtimeoutpattern, NIScdtimeoutpattern_mask, 0x28);
    if (ptr) {
        u16 jmp = _lw((u32)ptr + 32) & 0xffff;
        _sw(0x10000000 | jmp, (u32)ptr + 32);
    }
}

// Armored Core 9 Breaker generic USB patch
static void AC9B_generic_patches(void)
{
    static u32 AC9Bpattern[] = {
        0x8e450000,
        0x0220202d,
        0x0c000000,
        0x0000302d,
        0x04410003,
        0x00000000,
        0x10000005,
        0x2402ffff,
        0x8e020000,
        0x1040fff6};
    static const u32 AC9Bpattern_mask[] = {
        0xffffffff,
        0xffffffff,
        0xfc000000,
        0xffffffff,
        0xffffffff,
        0xffffffff,
        0xffffffff,
        0xffffffff,
        0xffffffff,
        0xffffffff};
    u32 *ptr;

    ptr = find_pattern_with_mask((u32 *)0x00100000, 0x01e00000, AC9Bpattern, AC9Bpattern_mask, 0x28);
    if (ptr)
        _sw(0, (u32)ptr + 36);
}

static int delayed_cdRead(u32 lsn, u32 nsectors, void *buf, int *mode)
{
    int r;
    unsigned int count;

    r = cdRead(lsn, nsectors, buf, mode);
    count = g_delay_cycles;
    while (count--)
        asm("nop\nnop\nnop\nnop");

    return r;
}

static void generic_delayed_cdRead_patches(u32 patch_addr, u32 delay_cycles)
{
    // set configureable delay cycles
    g_delay_cycles = delay_cycles;

    // get original cdRead() pointer
    cdRead = (void *)FNADDR(_lw(patch_addr));

    // overwrite with a JAL to our delayed_cdRead function
    _sw(JAL((u32)delayed_cdRead), patch_addr);
}

static int (*capcom_lmb)(void *modpack_addr, int mod_index, int mod_argc, char **mod_argv);

static void apply_capcom_protection_patch(void *modpack_addr, int mod_index, int mod_argc, char **mod_argv)
{
    u32 iop_addr = _lw((u32)modpack_addr + (mod_index << 3) + 8);
    u32 opcode = 0x10000025;
    SyncDCache((void *)opcode, (void *)((unsigned int)&opcode + sizeof(opcode)));
    smem_write((void *)(iop_addr + 0x270), (void *)&opcode, sizeof(opcode));

    capcom_lmb(modpack_addr, mod_index, mod_argc, mod_argv);
}

static void generic_capcom_protection_patches(u32 patch_addr)
{
    capcom_lmb = (void *)FNADDR(_lw(patch_addr));
    _sw(JAL((u32)apply_capcom_protection_patch), patch_addr);
}

static void Invoke_CRSGUI_Start(void)
{
    int (*pCRSGUI_Start)(int arg1, int arg2) = (void *)0x003054b0;

    pCRSGUI_Start(*(int *)0x0078f79c, 0);
}

static void SDF_Macross_patch(void)
{
    /*  Choujikuu Yousai Macross appears to have a rather large problem with it: it appears to use its GUI before initialization is completed.
        I did not attempt to figure out whether it's really a timing problem (whereby this happens before initialization is completed by another thread... if there is one),
        or if its normal functionality was the result of pure luck that SEGA had.

        The problems that it has are quite evident when this game is run within PCSX2. I still do not know why DECI2 does not detect the TLB exception when it
        dereferences NULL pointers, but it might have something to do with the game accessing the debug registers (PCSX2's logs indicate that). */

    _sw(JMP((unsigned int)&Invoke_CRSGUI_Start), 0x001f8520);
}

extern void SRWI_IncrementCntrlFlag(void);

static void SRWI_IMPACT_patches(void)
{
    // Phase 1    - Replace all segments of code that increment cntrl_flag with a multithread-safe implementation.
    // In cdvCallBack()
    _sw(JAL((unsigned int)&SRWI_IncrementCntrlFlag), 0x0021e840);
    _sw(0x00000000, 0x0021e84c);
    _sw(0x00000000, 0x0021e854);
    // In cdvMain()
    _sw(0x00000000, 0x00220ac8);
    _sw(JAL((unsigned int)&SRWI_IncrementCntrlFlag), 0x00220ad0);
    _sw(0x00000000, 0x00220ad8);
    _sw(JAL((unsigned int)&SRWI_IncrementCntrlFlag), 0x00220b20);
    _sw(0x00000000, 0x00220b28);
    _sw(0x00000000, 0x00220b30);
    _sw(JAL((unsigned int)&SRWI_IncrementCntrlFlag), 0x00220ba0);
    _sw(0x00000000, 0x00220ba8);

    /* Phase 2
        sceCdError() will be polled continuously until it succeeds in retrieving the CD/DVD drive status.
        However, the callback thread has a higher priority than the main thread
        and this might result in a freeze because the main thread wouldn't ever release the libcdvd semaphore, and so calls to sceCdError() by the callback thread wouldn't succeed.
        This problem occurs more frequently than the one addressed above.

        Since the PlayStation 2 EE uses non-preemptive multitasking, we can solve this problem by lowering the callback thread's priority th below the main thread.
        The problem is solved because the main thread can then interrupt the callback thread until it has completed its tasks.    */
    // In cdvCallBack()
    _sw(0x24040060, 0x0021e944); // addiu $a0, $zero, 0x60 (Set the CD/DVD callback thread's priority to 0x60)
}

void RnC3_AlwaysAllocMem(void);

static void RnC3_UYA_patches(void *address)
{
    unsigned int word1, word2;

    /*  Preserve the pointer to the allocated IOP RAM.
        This game's main executable is obfuscated and/or compressed in some way,
        but thankfully the segment that needs to be patched is just offset by 1 byte.

        It contains an IOP module that seems to load other modules (iop_stash_daemon),
        which unfortunately seems to be the heart of its anti-emulator protection system.
        It (and the EE-side code) appears to be playing around with a pointer to IOP RAM,
        based on the modules that are loaded.

        Right before this IOP module is loaded with a custom LoadModuleBuffer function, the game will allocate a large buffer on the IOP.
        This buffer is then used for loading iop_stash_daemon, which also uses it to load other modules before freeing it.
        Unfortunately, the developers appear to have hardcoded the pointer, rather than using the return value of sceAllocSysMemory().

        This module will also check for the presence of bit 29 in the pointer. If it's absent, then the code will not allocate memory and the game will freeze after the first cutscene in Veldin.
        Like with crazyc's original patch, this branch here will have to be adjusted:
            beqz $s7, 0x13
        ... to be:
            beqz $s7, 0x01

        iop_stash_daemon will play with the pointer in the following ways, based on each module it finds:
            1. if it's a module with no name (first 4 characters are 0s), left-shift once.
            2. if it's a module beginning with "Deci", left-shift once.
            3. if it's a module beginning with "cdvd", right-shift once.
        Otherwise, nothing is done for the module.

        Only modules up to before the 3rd last will be considered.

        For us, it's about preserving the pointer to the allocated buffer and to adjust it accordingly:
            For TOOL units, there are 6 DECI2 modules and 2 libcdvd modules. Therefore the pointer should be right-shifted by 4.
            For retail units, there are 2 libcdvd modules. Therefore the pointer should be left-shifted by 2.    */

    word1 = JAL((unsigned int)&RnC3_AlwaysAllocMem);
#ifdef _DTL_T10000
    word2 = 0x00021903; // sra $v1, $v0, 4    For DTL-T10000.
#else
    word2 = 0x00021880; // sll $v1, $v0, 2    For retail sets.
#endif

    memcpy(address, &word1, 4);
    memcpy((u8 *)address + 8, &word2, 4);
}

static void (*pZZscePadEnd)(void);
static void (*pZZInitIOP)(void);

static void ZombieZone_preIOPInit(void)
{
    pZZscePadEnd();
    pZZInitIOP();
}

static void ZombieZone_patches(unsigned int address)
{
    static const unsigned int ZZpattern[] = {
        0x2403000f, // addiu v1, zero, $000f
        0x24500000, // addiu s0, v0, xxxx
        0x3c040000, // lui a0, xxxx
        0xffbf0020, // sd ra, $0020(sp)
    };
    static const unsigned int ZZpattern_mask[] = {
        0xffffffff,
        0xffff0000,
        0xffff0000,
        0xffffffff};
    u32 *ptr;

    // Locate scePadEnd().
    ptr = find_pattern_with_mask((u32 *)0x001c0000, 0x01f00000, ZZpattern, ZZpattern_mask, sizeof(ZZpattern));
    if (ptr) {
        pZZInitIOP = (void *)FNADDR(_lw(address));
        pZZscePadEnd = (void *)(ptr - 3);

        _sw(JAL((unsigned int)&ZombieZone_preIOPInit), address);
    }
}

static void DotHack_patches(const char *path)
{ //.hack (PAL) has a multi-language selector that boots the main ELF. However, it does not call scePadEnd() before LoadExecPS2()
    // We only want to patch the language selector and nothing else!
    static u32 patch[] = {
        0x00000000, // jal scePadEnd()
        0x00000000, // nop
        0x27a40020, // addiu $a0, $sp, $0020 (Contains boot path)
        0x0000282d, // move $a1, $zero
        0x00000000, // j LoadExecPS2()
        0x0000302d, // move $a2, $zero
    };
    u32 *ptr, *pPadEnd, *pLoadExecPS2;

    if (_strcmp(path, "cdrom0:\\SLES_522.37;1") == 0) {
        ptr = (void *)0x0011a5fc;
        pPadEnd = (void *)0x00119290;
        pLoadExecPS2 = (void *)FNADDR(ptr[2]);
    } else if (_strcmp(path, "cdrom0:\\SLES_524.67;1") == 0) {
        ptr = (void *)0x0011a8bc;
        pPadEnd = (void *)0x00119550;
        pLoadExecPS2 = (void *)FNADDR(ptr[2]);
    } else if (_strcmp(path, "cdrom0:\\SLES_524.68;1") == 0) {
        ptr = (void *)0x00111d34;
        pPadEnd = (void *)0x001109b0;
        pLoadExecPS2 = (void *)FNADDR(ptr[3]);
    } else if (_strcmp(path, "cdrom0:\\SLES_524.69;1") == 0) {
        ptr = (void *)0x00111d34;
        pPadEnd = (void *)0x001109b0;
        pLoadExecPS2 = (void *)FNADDR(ptr[3]);
    } else {
        ptr = NULL;
        pPadEnd = NULL;
        pLoadExecPS2 = NULL;
    }

    if (ptr != NULL && pPadEnd != NULL && pLoadExecPS2 != NULL) {
        patch[0] = JAL((u32)pPadEnd);
        patch[4] = JMP((u32)pLoadExecPS2);
        memcpy(ptr, patch, sizeof(patch));
    }
}

#ifdef IREMSSND_PATCH // for SOS
static int SOS_SifLoadModuleHook(const char *path, int arg_len, const char *args, int *modres, int fno)
{
    int (*_pSifLoadModule)(const char *path, int arg_len, const char *args, int *modres, int fno);
    void *(*pSifAllocIopHeap)(int size);
    int (*pSifFreeIopHeap)(void *addr);
    int (*pSifLoadModuleBuffer)(void *ptr, int arg_len, const char *args);
    void *iopmem;
    SifDmaTransfer_t sifdma;
    int dma_id, ret, ret2;
    void *iremsndpatch_irx;
    unsigned int iremsndpatch_irx_size;
    char modIdStr[3];

    switch (g_mode) {
        case 0: // NTSC-J
            _pSifLoadModule = (void *)0x001d0680;
            pSifAllocIopHeap = (void *)0x001cfc30;
            pSifFreeIopHeap = (void *)0x001cfd20;
            pSifLoadModuleBuffer = (void *)0x001d0640;
            break;
        case 1: // NTSC-U/C
            _pSifLoadModule = (void *)0x001d0580;
            pSifAllocIopHeap = (void *)0x001cfb30;
            pSifFreeIopHeap = (void *)0x001cfc20;
            pSifLoadModuleBuffer = (void *)0x001d0540;
            break;
        case 2: // PAL
            _pSifLoadModule = (void *)0x001d11c0;
            pSifAllocIopHeap = (void *)0x001d0770;
            pSifFreeIopHeap = (void *)0x001d0860;
            pSifLoadModuleBuffer = (void *)0x001d1180;
            break;
        default:
            _pSifLoadModule = NULL;
            pSifAllocIopHeap = NULL;
            pSifFreeIopHeap = NULL;
            pSifLoadModuleBuffer = NULL;
            // Should not happen.
            asm volatile("break\n");
    }

    ret = _pSifLoadModule(path, arg_len, args, modres, fno);

    if ((ret >= 0) && (_strcmp(path, "cdrom0:\\IOP\\IREMSND.IRX;1") == 0)) {
        GetOPLModInfo(EECORE_MODULE_ID_IOP_PATCH, &iremsndpatch_irx, &iremsndpatch_irx_size);

        iopmem = pSifAllocIopHeap(iremsndpatch_irx_size);
        if (iopmem != NULL) {
            sifdma.src = iremsndpatch_irx;
            sifdma.dest = iopmem;
            sifdma.size = iremsndpatch_irx_size;
            sifdma.attr = 0;
            do {
                dma_id = SifSetDma(&sifdma, 1);
            } while (!dma_id);

            modIdStr[0] = NIBBLE2CHAR((ret >> 4) & 0xF);
            modIdStr[1] = NIBBLE2CHAR(ret & 0xF);
            modIdStr[2] = '\0';

            do {
                ret2 = pSifLoadModuleBuffer(iopmem, sizeof(modIdStr), modIdStr);
            } while (ret2 < 0);

            pSifFreeIopHeap(iopmem);
        } else
            asm volatile("break\n");
    }

    return ret;
}

static void SOSPatch(int region)
{
    g_mode = region;

    switch (region) { // JAL SOS_SifLoadModuleHook - replace call to _SifLoadModule.
        case 0:       // NTSC-J
            _sw(JAL((u32)&SOS_SifLoadModuleHook), 0x001d08b4);
            break;
        case 1: // NTSC-U/C
            _sw(JAL((u32)&SOS_SifLoadModuleHook), 0x001d07b4);
            break;
        case 2: // PAL
            // _sw(JAL((u32)&SOS_SifLoadModuleHook), 0x001d13f4);
            break;
    }
}
#endif

static void VirtuaQuest_patches(void)
{
    /* Move module storage to 0x01FC7000.

       Ideal end of memory: 0x02000000 - (0x000D0000 - 0x00097000) = 0x02000000 - 0x39000 = 0x01FC7000.
       The main thread's stack size is 0x18000.
       Note: this means that the stack will overwrite the module storage and hence further IOP reboots are not possible.
       However, carving out memory for the modules results in a NULL pointer being passed to memset(). */

    // Fix the stack base pointer for SetupThread(), so that the EE kernel will not reserve 4KB.
    // 0x02000000 - 0x18000 = 0x01FE8000
    _sw(0x3c0501fe, 0x000a019c); // lui $a1, $01fe
    _sw(0x34a58000, 0x000a01b0); // ori a1, a1, $8000

    // Change end of memory pointer (game will subtract 0x18000 from it).
    // 0x02000000 - (0x39000 - 0x18000) = 0x1FDF000
    _sw(0x3c0301fd, 0x000c565c); // lui $v1, 0x01fd
    _sw(0x3463f000, 0x000c566c); // ori $v1, $v1, 0xf000
}

#ifdef APEMOD_PATCH // for UltProPinball
enum ULTPROPINBALL_ELF {
    ULTPROPINBALL_ELF_MAIN,
    ULTPROPINBALL_ELF_BR,
    ULTPROPINBALL_ELF_FJ,
    ULTPROPINBALL_ELF_TS,
};

static int UltProPinball_SifLoadModuleHook(const char *path, int arg_len, const char *args)
{
    int (*pSifLoadModule)(const char *path, int arg_len, const char *args);
    void *(*pSifAllocIopHeap)(int size);
    int (*pSifFreeIopHeap)(void *addr);
    int (*pSifLoadModuleBuffer)(void *ptr, int arg_len, const char *args);
    void *iopmem;
    SifDmaTransfer_t sifdma;
    int dma_id, ret;
    void *apemodpatch_irx;
    unsigned int apemodpatch_irx_size;

    switch (g_mode & 0xf) {
        case ULTPROPINBALL_ELF_MAIN:
            pSifLoadModule = (void *)0x001d0140;
            pSifAllocIopHeap = (void *)0x001cf278;
            pSifFreeIopHeap = (void *)0x001cf368;
            pSifLoadModuleBuffer = (void *)0x001cfed8;
            break;
        case ULTPROPINBALL_ELF_BR:
            pSifLoadModule = (void *)0x0023aa80;
            pSifAllocIopHeap = (void *)0x00239bb8;
            pSifFreeIopHeap = (void *)0x00239ca8;
            pSifLoadModuleBuffer = (void *)0x0023a818;
            break;
        case ULTPROPINBALL_ELF_FJ:
            pSifLoadModule = (void *)0x00224740;
            pSifAllocIopHeap = (void *)0x00223878;
            pSifFreeIopHeap = (void *)0x00223968;
            pSifLoadModuleBuffer = (void *)0x002244d8;
            break;
        case ULTPROPINBALL_ELF_TS:
            pSifLoadModule = (void *)0x00233040;
            pSifAllocIopHeap = (void *)0x00232178;
            pSifFreeIopHeap = (void *)0x00232268;
            pSifLoadModuleBuffer = (void *)0x00232dd8;
            break;
        default:
            pSifLoadModule = NULL;
            pSifAllocIopHeap = NULL;
            pSifFreeIopHeap = NULL;
            pSifLoadModuleBuffer = NULL;
            // Should not happen.
            asm volatile("break\n");
    }

    if (_strcmp(path, "cdrom0:\\APEMOD.IRX;1") != 0)
        ret = pSifLoadModule(path, arg_len, args);
    else {
        GetOPLModInfo(EECORE_MODULE_ID_IOP_PATCH, &apemodpatch_irx, &apemodpatch_irx_size);

        iopmem = pSifAllocIopHeap(apemodpatch_irx_size);
        if (iopmem != NULL) {
            sifdma.src = apemodpatch_irx;
            sifdma.dest = iopmem;
            sifdma.size = apemodpatch_irx_size;
            sifdma.attr = 0;
            do {
                dma_id = SifSetDma(&sifdma, 1);
            } while (!dma_id);

            do {
                ret = pSifLoadModuleBuffer(iopmem, strlen(path) + 1, path);
            } while (ret < 0);

            pSifFreeIopHeap(iopmem);
        } else {
            ret = -1;
            asm volatile("break\n");
        }
    }

    return ret;
}

static void UltProPinballPatch(const char *path)
{
    if (_strcmp(path, "cdrom0:\\SLES_535.08;1") == 0) {
        _sw(JAL((u32)&UltProPinball_SifLoadModuleHook), 0x0012e47c);
        g_mode = ULTPROPINBALL_ELF_MAIN;
    } else if (_strcmp(path, "cdrom0:\\BR.ELF;1") == 0) {
        _sw(JAL((u32)&UltProPinball_SifLoadModuleHook), 0x00196a3c);
        g_mode = ULTPROPINBALL_ELF_BR;
    } else if (_strcmp(path, "cdrom0:\\FJ.ELF;1") == 0) {
        _sw(JAL((u32)&UltProPinball_SifLoadModuleHook), 0x00180f2c);
        g_mode = ULTPROPINBALL_ELF_FJ;
    } else if (_strcmp(path, "cdrom0:\\TS.ELF;1") == 0) {
        _sw(JAL((u32)&UltProPinball_SifLoadModuleHook), 0x0018d434);
        g_mode = ULTPROPINBALL_ELF_TS;
    }
}
#endif

static void EutechnyxWakeupTIDPatch(u32 addr)
{ // Eutechnyx games have the main thread ID hardcoded for a call to WakeupThread().
    // addiu $a0, $zero, 1
    // This breaks when the thread IDs change after IGR is used.

    /*
    MTV Pimp My Ride uses same serial for v1.00 and v2.00 of USA release.
    We need to tell which offsets to use.
    */
    if (_strcmp(GameID, "SLUS_215.80") == 0) {
        // Check version v1.00 by default.
        if (*(vu16 *)addr == 1) {
            *(vu16 *)addr = (u16)GetThreadId();
            return;
        }

        // Now check if v2.00.
        if (*(vu16 *)(PATCH_MTV_PMR_V200_ADDR) == 1) {
            *(vu16 *)(PATCH_MTV_PMR_V200_ADDR) = (u16)GetThreadId();
        }
        return;
    }

    /*
    Same problem with SRS: Street Racing Syndicate
    The patch already exists but it was for v1.03 of the game so if it was trying to boot v2.00 then it would be wrong patched. This handles both cases correctly.
    */
    if (_strcmp(GameID, "SLUS_205.82") == 0) {
        // Check version v1.03 by default.
        if (*(vu16 *)addr == 1) {
            *(vu16 *)addr = (u16)GetThreadId();
            return;
        }

        // Now check if v2.00.
        if (*(vu16 *)(PATCH_SRS_V200_ADDR) == 1) {
            *(vu16 *)(PATCH_SRS_V200_ADDR) = (u16)GetThreadId();
        }
        return;
    }

    *(vu16 *)addr = (u16)GetThreadId();
}

static void ProSnowboarderPatch(void)
{ // Shaun Palmer's Pro Snowboarder incorrectly uses the main thread ID as the priority, causing a deadlock when the main thread ID changes (ID != priority)
    // Replace all jal GetThreadId() with a li $v0, 1, whereby 1 is the main thread's priority (never changed by game).
    static const unsigned int pattern[] = {
        0x240300ff, // addiu $v1, $zero, 0xff
        0x3c038080, // li $v0, 0x8080
        0x34638081, // ori $v1, $v1, 0x8181
        0x00650018, // mult $v1, $a1
    };
    static const unsigned int pattern_mask[] = {
        0xffffffff,
        0xffffffff,
        0xffffffff,
        0xffffffff};
    u32 *ptr, *ptr2, *ptr3;

    // Locate the calls to GetThreadId().
    ptr = find_pattern_with_mask((u32 *)0x00180000, 0x00280000, pattern, pattern_mask, sizeof(pattern));
    if (ptr) {
        ptr2 = find_pattern_with_mask(ptr + 4, 0x00280000, pattern, pattern_mask, sizeof(pattern));

        if (ptr2) {
            ptr3 = find_pattern_with_mask(ptr2 + 4, 0x00280000, pattern, pattern_mask, sizeof(pattern));

            if (ptr3) {
                *(vu32 *)&ptr[-12] = 0x24020001; // addiu $v0, $zero, 1
                *(vu32 *)&ptr2[-9] = 0x24020001; // addiu $v0, $zero, 1
                *(vu32 *)&ptr3[-9] = 0x24020001; // addiu $v0, $zero, 1
            }
        }
    }
}

#ifdef F2TECH_PATCH // for ShadowMan2
static int ShadowMan2_SifLoadModuleHook(const char *path, int arg_len, const char *args)
{
    // int (*pSifLoadModule)(const char *path, int arg_len, const char *args);
    void *(*pSifAllocIopHeap)(int size);
    int (*pSifFreeIopHeap)(void *addr);
    int (*pSifLoadModuleBuffer)(void *ptr, int arg_len, const char *args);
    void *iopmem;
    SifDmaTransfer_t sifdma;
    int dma_id, ret;
    void *f2techioppatch_irx;
    unsigned int f2techioppatch_irx_size;

    switch (g_mode) {
        case 1: // NTSC-U/C
            // pSifLoadModule = (void *)0x00234188;
            pSifAllocIopHeap = (void *)0x239df0;
            pSifFreeIopHeap = (void *)0x239f58;
            pSifLoadModuleBuffer = (void *)0x00233f20;
            break;
        case 2: // PAL
            // pSifLoadModule = (void *)0x002336c8;
            pSifAllocIopHeap = (void *)0x00239330;
            pSifFreeIopHeap = (void *)0x00239498;
            pSifLoadModuleBuffer = (void *)0x00233460;
            break;
        case 3: // PAL German
            // pSifLoadModule = (void *)0x00233588;
            pSifAllocIopHeap = (void *)0x002391f0;
            pSifFreeIopHeap = (void *)0x00239358;
            pSifLoadModuleBuffer = (void *)0x00233320;
            break;
        default:
            // pSifLoadModule = NULL;
            pSifAllocIopHeap = NULL;
            pSifFreeIopHeap = NULL;
            pSifLoadModuleBuffer = NULL;
            // Should not happen.
            asm volatile("break\n");
    }

    GetOPLModInfo(EECORE_MODULE_ID_IOP_PATCH, &f2techioppatch_irx, &f2techioppatch_irx_size);

    iopmem = pSifAllocIopHeap(f2techioppatch_irx_size);
    if (iopmem != NULL) {
        sifdma.src = f2techioppatch_irx;
        sifdma.dest = iopmem;
        sifdma.size = f2techioppatch_irx_size;
        sifdma.attr = 0;
        do {
            dma_id = SifSetDma(&sifdma, 1);
        } while (!dma_id);

        do {
            ret = pSifLoadModuleBuffer(iopmem, strlen(path) + 1, path);
        } while (ret < 0);

        pSifFreeIopHeap(iopmem);
    } else {
        ret = -1;
        asm volatile("break\n");
    }

    return ret;
}

static void ShadowMan2Patch(int region)
{
    g_mode = region;

    switch (region) { // JAL ShadowMan2_SifLoadModuleHook.
        case 1:       // NTSC-U/C
            _sw(JAL((u32)&ShadowMan2_SifLoadModuleHook), 0x001d2838);
            break;
        case 2: // PAL
            _sw(JAL((u32)&ShadowMan2_SifLoadModuleHook), 0x001d2768);
            break;
        case 3: // PAL German
            _sw(JAL((u32)&ShadowMan2_SifLoadModuleHook), 0x001d2650);
            break;
    }
}
#endif

static void HarvestMoonAWLPatch(int region)
{
    /* Harvest Moon create alot of threads. When the game gets stuck, all the threads are either suspended or waiting.
   What seems to be happening is thread 16 trying to wake up the main thread. However, this might be failing because
   the game also has additional checks around WakeupThread, to prevent a thread from waking up another thread
   if the thread to be woken up is not in a Wait state. So if the main thread has not slept, the main thread will
   enter a state of eternal sleep once it does because thread 16 will be prohibited from waking it up.

   Thread 16 will not make further attempts to wake up the main thread as it will also sleep,
   regardless of whether the main thread was successfully woken up or not.

   Disabling the extra checks seem to make the glitch go away, but I don't know why they added extra code like that.
   By design, calling WakeupThread on a thread that is not sleeping will increase the WakeUpCount value of the thread,
   which will cause it to inhibit a number of calls to SleepThread() by that amount
   (which I think is fair - if the thread has to be woken up, it should be).

   The mistake may be made in various places, but seems to only affect the game's initial loading screen. */

    switch (region) {
        case 1: // NTSC-U/C
        case 2: // PAL
            _sw(0x00000000, 0x0011b694);
            _sw(0x00000000, 0x0011b6c0);
            _sw(0x00000000, 0x0011b6c4);
            break;
        case 0: // NTSC-J
            _sw(0x00000000, 0x0011b634);
            _sw(0x00000000, 0x0011b660);
            _sw(0x00000000, 0x0011b664);
    }
}

void apply_patches(const char *path)
{
    const patchlist_t *p;
    // Some patches hack into specific ELF files
    // make sure the filename and gameid match for those patches
    // This prevents games with multiple ELF's from being corrupted by the patch
    int file_eq_gameid = !_strncmp(&path[8], GameID, 11); // starting after 'cdrom0:\'

    // if there are patches matching game name/mode then fill the patch table
    for (p = patch_list; p->game; p++) {
        if ((!_strcmp(GameID, p->game)) && (p->mode & GameMode)) {
            switch (p->patch.addr) {
                case PATCH_GENERIC_NIS:
                    NIS_generic_patches();
                    break;
                case PATCH_GENERIC_AC9B:
                    AC9B_generic_patches();
                    break;
                case PATCH_GENERIC_SLOW_READS:
                    if (file_eq_gameid)
                        generic_delayed_cdRead_patches(p->patch.check, p->patch.val); // slow reads generic patch
                    break;
                case PATCH_SDF_MACROSS:
                    if (file_eq_gameid)
                        SDF_Macross_patch();
                    break;
                case PATCH_GENERIC_CAPCOM:
                    if (file_eq_gameid)
                        generic_capcom_protection_patches(p->patch.val); // Capcom anti cdvd emulator protection patch
                    break;
                case PATCH_SRW_IMPACT:
                    if (file_eq_gameid)
                        SRWI_IMPACT_patches();
                    break;
                case PATCH_RNC_UYA:
                    if (file_eq_gameid)
                        RnC3_UYA_patches((unsigned int *)p->patch.val);
                    break;
                case PATCH_ZOMBIE_ZONE:
                    if (file_eq_gameid)
                        ZombieZone_patches(p->patch.val);
                    break;
                case PATCH_DOT_HACK:
                    DotHack_patches(path);
                    break;
                case PATCH_SOS:
#ifdef IREMSSND_PATCH
                    SOSPatch(p->patch.val);
#endif
                    break;
                case PATCH_VIRTUA_QUEST:
                    if (file_eq_gameid)
                        VirtuaQuest_patches();
                    break;
                case PATCH_ULT_PRO_PINBALL:
#ifdef APEMOD_PATCH
                    UltProPinballPatch(path);
#endif
                    break;
                case PATCH_EUTECHNYX_WU_TID:
                    if (file_eq_gameid)
                        EutechnyxWakeupTIDPatch(p->patch.val);
                    break;
                case PATCH_PRO_SNOWBOARDER:
                    ProSnowboarderPatch();
                    break;
                case PATCH_SHADOW_MAN_2:
#ifdef F2TECH_PATCH
                    ShadowMan2Patch(p->patch.val);
#endif
                    break;
                case PATCH_HARVEST_MOON_AWL:
                    HarvestMoonAWLPatch(p->patch.val);
                    break;
                default: // Single-value patches
                    if (_lw(p->patch.addr) == p->patch.check)
                        _sw(p->patch.val, p->patch.addr);
            }
        }
    }
}
