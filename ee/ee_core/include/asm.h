/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review Open-Ps2-Loader README & LICENSE files for further details.

  Some parts of the code are taken from HD Project by Polo
*/

#ifndef ASM_H
#define ASM_H

#include <tamtypes.h>
#include <sifdma.h>

u32 Hook_SifSetDma(SifDmaTransfer_t *sdd, s32 len);
void CleanExecPS2(void *epc, void *gp, int argc, char **argv);

#endif /* ASM */
