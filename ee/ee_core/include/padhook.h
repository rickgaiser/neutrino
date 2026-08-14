/*
  padhook.h - In-Game Reset (IGR) for Neutrino

  Ported from Open-PS2-Loader's ee_core padhook (GPL / AFL 3.0):
    Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
    Copyright 2006-2008 Polo
  PadOpen hooking inspired by ps2rd (jimmikaelkael, misfire).

  Reset-only Neutrino port: the combo L1+L2+R1+R2+SELECT+START or the
  console's front-panel reset/power button returns to the NHDDL dashboard.
*/

#ifndef PADHOOK_H
#define PADHOOK_H

#include <tamtypes.h>

#define PADOPEN_HOOK  0
#define PADOPEN_CHECK 1

// Shared with the resident syscall hooks. A zero padOpen_hooked value tells
// the ExecPS2/CreateThread hooks to retry after dynamically loaded game code
// appears. disable_padOpen_hook guards IOP-reset/module-loading windows.
extern int padOpen_hooked;
extern int disable_padOpen_hook;

// The resident worker is intentionally visible to the kernel syscall guards.
// Games may manage arbitrary thread IDs; only this one ID is protected while
// the current game-side IGR instance is active.
extern volatile int IGR_Thread_ID;

// Scans [mem_start, mem_end) for the game's libpad/libpad2 pad-open function
// and (mode PADOPEN_HOOK) redirects its callers to our hook. Returns patched.
int Install_PadOpen_Hook(u32 mem_start, u32 mem_end, int mode);

// Installs the IGR thread and the VBLANK-END interrupt handler (idempotent).
void Install_IGR(void);

// Resets the stored thread/handler IDs (call once before first install).
void Reset_Padhook(void);

// CDVD power/reset button registers (same hardware path used by OPL IGR).
#define CDVD_R_NDIN ((volatile u8 *)0xBF402005)
#define CDVD_R_POFF ((volatile u8 *)0xBF402008)
#define CDVD_R_SCMD ((volatile u8 *)0xBF402016)
#define CDVD_R_SDIN ((volatile u8 *)0xBF402017)

typedef struct
{
    u32 option;
    int port;
    int slot;
    int number;
    u8 name[16];
} pad2socketparam_t;

typedef struct
{
    u16 libpad;
    u16 libversion;
    u8 *pad_buf;
    int vb_count;
    int pos_combo1;
    int pos_combo2;
    int pos_state;
    int pos_frame;
    u8 combo_type;
    u8 prev_frame;
    u8 live;
} paddata_t;

typedef struct
{
    u32 *pattern;
    u32 *mask;
    int size;
    u16 type; // libpad or libpad2
    u16 version;
} pattern_t;

#define IGR_LIBPAD_NONE 0
#define IGR_LIBPAD      1
#define IGR_LIBPAD2     2

#define IGR_PAD_STABLE_V1 0x06
#define IGR_PAD_STABLE_V2 0x01

#define IGR_COMBO_R1_L1_R2_L2  0xF0
#define IGR_COMBO_START_SELECT 0xF6

#define NB_PADOPEN_PATTERN 7

#endif
