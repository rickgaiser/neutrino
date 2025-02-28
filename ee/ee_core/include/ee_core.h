/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.

  Some parts of the code are taken from HD Project by Polo
*/

#ifndef EE_CORE_H
#define EE_CORE_H

#include <tamtypes.h>
#include <kernel.h>
#include <stdio.h>
#include <iopheap.h>
#include <ps2lib_err.h>
#include <sifrpc.h>
#include <string.h>
#include <sbv_patches.h>
#include <smem.h>
#include <smod.h>

#ifdef __EESIO_DEBUG
#define DPRINTF(args...) _print(args)
#define DINIT()          InitDebug()
#else
#define DPRINTF(args...) \
    do {                 \
    } while (0)
#define DINIT() \
    do {        \
    } while (0)
#endif

extern int set_reg_hook;
extern int new_iop_reboot_count;

#define BDM_ILK_MODE (1<<0)
#define BDM_M4S_MODE (1<<1)
#define BDM_USB_MODE (1<<2)
#define BDM_UDP_MODE (1<<3)
#define BDM_ATA_MODE (1<<4)
#define BDM_NOP_MODE (1<<31)

#endif
