/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review Open-Ps2-Loader README & LICENSE files for further details.

  Some parts of the code are taken from HD Project by Polo
*/

#ifndef SYSHOOK_H
#define SYSHOOK_H

#include <tamtypes.h>

void services_start(void);
void services_exit(void);
void Install_Kernel_Hooks(void);

extern u32 (*Old_SifSetDma)(SifDmaTransfer_t *sdd, s32 len);
extern int (*Old_SifSetReg)(u32 register_num, int register_value);

void sysLoadElf(char *filename, int argc, char **argv);

#endif /* SYSHOOK */
