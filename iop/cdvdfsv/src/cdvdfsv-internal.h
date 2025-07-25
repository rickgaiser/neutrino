/*
  Copyright 2009, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review open-ps2-loader README & LICENSE files for further details.
*/

#ifndef __CDVDFSV_INTERNAL__
#define __CDVDFSV_INTERNAL__

#include <intrman.h>
#include <loadcore.h>
#include <stdio.h>
#include <sifcmd.h>
#include <sifman.h>
#include <sysclib.h>
#include <thbase.h>
#include <thevent.h>
#include <thsemap.h>
#include <cdvdman.h>

#include "cdvdman_opl.h"
#include "mprintf.h"

#define MODNAME "cdvd_ee_driver"

extern void cdvdfsv_register_scmd_rpc(SifRpcDataQueue_t *rpc_DQ);
extern void cdvdfsv_register_ncmd_rpc(SifRpcDataQueue_t *rpc_DQ);
extern void cdvdfsv_register_searchfile_rpc(SifRpcDataQueue_t *rpc_DQ);
extern void sysmemSendEE2(void *src0, void *dst0, int size0, void *src1, void *dst1, int size1);
extern void sysmemSendEE(void *src0, void *dst0, int size0);
extern int sceCdChangeThreadPriority(int priority);

extern u8 *cdvdfsv_buf;
extern int cdvdfsv_size;
extern int cdvdfsv_sectors;

#endif
