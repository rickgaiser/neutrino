/*
  Copyright 2009, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review open-ps2-loader README & LICENSE files for further details.
*/

#include "cdvdfsv-internal.h"

IRX_ID(MODNAME, 2, 2);

typedef struct
{
    int func_ret;
    int cdvdfsv_ver;
    int cdvdman_ver;
    int debug_mode;
} cdvdinit_res_t;

// internal prototypes
static void init_thread(void *args);
static void cdvdfsv_startrpcthreads(void);
static void cdvdfsv_rpc0_th(void *args);
static void cdvdfsv_rpc1_th(void *args);
static void cdvdfsv_rpc2_th(void *args);
static void *cbrpc_cdinit(int fno, void *buf, int size);
static void *cbrpc_cddiskready(int fno, void *buf, int size);
static void *cbrpc_cddiskready2(int fno, void *buf, int size);
static void *cbrpc_S596(int fno, void *buf, int size);

u8 *cdvdfsv_buf;

static SifRpcDataQueue_t rpc0_DQ;
static SifRpcDataQueue_t rpc1_DQ;
static SifRpcDataQueue_t rpc2_DQ;
static SifRpcServerData_t cdinit_rpcSD, cddiskready_rpcSD, cddiskready2_rpcSD;
static SifRpcServerData_t S596_rpcSD;

static u8 cdinit_rpcbuf[16];
static u8 cddiskready_rpcbuf[16];
static u8 cddiskready2_rpcbuf[16];
static u8 S596_rpcbuf[16];

static int rpc0_thread_id, rpc1_thread_id, rpc2_thread_id, rpc_sd_thread_id;

extern struct irx_export_table _exp_cdvdfsv;

//-------------------------------------------------------------------------
iop_library_t *ioplib_getByName(const char *name)
{
    iop_library_t *libptr;
    int i;

    // Get first loaded library
    libptr = GetLoadcoreInternalData()->let_next;
    // Loop through all loaded libraries
    while (libptr != NULL) {
        // Compare library name only
        for (i = 0; i < 8; i++) {
            if (libptr->name[i] != name[i])
                break;
        }

        // Return if match
        if (i == 8)
            return libptr;

        // Next library
        libptr = libptr->prev;
    }

    return NULL;
}

//-------------------------------------------------------------------------
int module_start(int argc, char *argv[])
{
    iop_library_t *lib_modload;
    iop_thread_t thread_param;

    M_DEBUG("%s\n", __FUNCTION__);

    RegisterLibraryEntries(&_exp_cdvdfsv);

    thread_param.attr = TH_C;
    thread_param.option = 0;
    thread_param.thread = &init_thread;
    thread_param.stacksize = 0x1000;
    thread_param.priority = 0x50;

    StartThread(CreateThread(&thread_param), NULL);

    lib_modload = ioplib_getByName("modload\0");
    if (lib_modload != NULL) {
        M_DEBUG("modload 0x%x detected\n", lib_modload->version);
        // Newer modload versions allow modules to be unloaded
        // Let modload know we support unloading
        if (lib_modload->version > 0x102)
            return MODULE_REMOVABLE_END;
    } else {
        M_DEBUG("modload not detected!\n");
    }

    return MODULE_RESIDENT_END;
}

//-------------------------------------------------------------------------
int module_stop(int argc, char *argv[])
{
    M_DEBUG("%s\n", __FUNCTION__);

    DeleteThread(rpc_sd_thread_id);
    DeleteThread(rpc0_thread_id);
    DeleteThread(rpc2_thread_id);
    DeleteThread(rpc1_thread_id);
    ReleaseLibraryEntries(&_exp_cdvdfsv);

    return MODULE_NO_RESIDENT_END;
}

//-------------------------------------------------------------------------
int _start(int argc, char *argv[])
{
    M_DEBUG("%s\n", __FUNCTION__);

    if (argc >= 0)
        return module_start(argc, argv);
    else
        return module_stop(-argc, argv);
}

//-------------------------------------------------------------------------
static void init_thread(void *args)
{
    sceSifInitRpc(0);

    cdvdfsv_buf = sceGetFsvRbuf();
    cdvdfsv_startrpcthreads();

    ExitDeleteThread();
}

//-------------------------------------------------------------------------
static void cdvdfsv_startrpcthreads(void)
{
    iop_thread_t thread_param;

    thread_param.attr = TH_C;
    thread_param.option = 0xABCD8001;
    thread_param.thread = (void *)cdvdfsv_rpc1_th;
    thread_param.stacksize = 0x1000; // Original: 0x1900. Its operations probably won't need so much space, since its functions will never trigger a callback.
    thread_param.priority = 0x51;

    rpc1_thread_id = CreateThread(&thread_param);
    StartThread(rpc1_thread_id, NULL);

    thread_param.attr = TH_C;
    thread_param.option = 0xABCD8002;
    thread_param.thread = (void *)cdvdfsv_rpc2_th;
    thread_param.stacksize = 0x1900;
    thread_param.priority = 0x51;

    rpc2_thread_id = CreateThread(&thread_param);
    StartThread(rpc2_thread_id, NULL);

    thread_param.attr = TH_C;
    thread_param.option = 0xABCD8000;
    thread_param.thread = (void *)cdvdfsv_rpc0_th;
    thread_param.stacksize = 0x1000; // Original: 0x800. Its operations probably won't need so much space, since its functions will never trigger a callback.
    thread_param.priority = 0x51;

    rpc0_thread_id = CreateThread(&thread_param);
    StartThread(rpc0_thread_id, NULL);
}

//-------------------------------------------------------------------------
static void cdvdfsv_rpc0_th(void *args)
{
    sceSifSetRpcQueue(&rpc0_DQ, GetThreadId());

    sceSifRegisterRpc(&S596_rpcSD, 0x80000596, &cbrpc_S596, S596_rpcbuf, NULL, NULL, &rpc0_DQ);
    sceSifRegisterRpc(&cddiskready2_rpcSD, 0x8000059c, &cbrpc_cddiskready2, cddiskready2_rpcbuf, NULL, NULL, &rpc0_DQ);

    sceSifRpcLoop(&rpc0_DQ);
}

//-------------------------------------------------------------------------
static void cdvdfsv_rpc1_th(void *args)
{
    // Starts cd Init rpc Server
    // Starts cd Disk ready rpc server
    // Starts SCMD rpc server;

    sceSifSetRpcQueue(&rpc1_DQ, GetThreadId());

    sceSifRegisterRpc(&cdinit_rpcSD, 0x80000592, &cbrpc_cdinit, cdinit_rpcbuf, NULL, NULL, &rpc1_DQ);
    cdvdfsv_register_scmd_rpc(&rpc1_DQ);
    sceSifRegisterRpc(&cddiskready_rpcSD, 0x8000059a, &cbrpc_cddiskready, cddiskready_rpcbuf, NULL, NULL, &rpc1_DQ);

    sceSifRpcLoop(&rpc1_DQ);
}

//-------------------------------------------------------------------------
static void cdvdfsv_rpc2_th(void *args)
{
    // Starts NCMD rpc server
    // Starts Search file rpc server

    sceSifSetRpcQueue(&rpc2_DQ, GetThreadId());

    cdvdfsv_register_ncmd_rpc(&rpc2_DQ);
    cdvdfsv_register_searchfile_rpc(&rpc2_DQ);

    sceSifRpcLoop(&rpc2_DQ);
}

//-------------------------------------------------------------------------
static void *cbrpc_cdinit(int fno, void *buf, int size)
{ // CD Init RPC callback
    cdvdinit_res_t *r = (cdvdinit_res_t *)buf;

    r->func_ret = sceCdInit(*(int *)buf);

    r->cdvdfsv_ver = 0x223;
    r->cdvdman_ver = 0x223;
    // r->debug_mode = 0xff;

    return buf;
}

//-------------------------------------------------------------------------
static void *cbrpc_cddiskready(int fno, void *buf, int size)
{ // CD Disk Ready RPC callback

    *(int *)buf = sceCdDiskReady((*(int *)buf == 0) ? 0 : 1);

    return buf;
}

//-------------------------------------------------------------------------
static void *cbrpc_cddiskready2(int fno, void *buf, int size)
{ // CD Disk Ready2 RPC callback

    *(int *)buf = sceCdDiskReady(0);

    return buf;
}

//--------------------------------------------------------------
static void *cbrpc_S596(int fno, void *buf, int size)
{
    int cdvdman_intr_ef, dummy;

    if (fno == 1) {
        cdvdman_intr_ef = sceCdSC(CDSC_GET_INTRFLAG, &dummy);
        ClearEventFlag(cdvdman_intr_ef, ~4);
        WaitEventFlag(cdvdman_intr_ef, 4, WEF_AND, NULL);
    }

    *(int *)buf = 1;
    return buf;
}

//--------------------------------------------------------------
void sysmemSendEE(void *buf, void *EE_addr, int size)
{
    SifDmaTransfer_t dmat;
    int oldstate, id;

    dmat.dest = (void *)EE_addr;
    dmat.size = size;
    dmat.src = (void *)buf;
    dmat.attr = 0;

    do {
        CpuSuspendIntr(&oldstate);
        id = sceSifSetDma(&dmat, 1);
        CpuResumeIntr(oldstate);
    } while (!id);

    while (sceSifDmaStat(id) >= 0)
        ;
}

//-------------------------------------------------------------------------
int sceCdChangeThreadPriority(int priority)
{
    if ((u32)(priority - 9) < 0x73) {
        if (priority == 9)
            priority = 10;

        ChangeThreadPriority(rpc0_thread_id, priority);
        ChangeThreadPriority(rpc2_thread_id, priority);
        ChangeThreadPriority(rpc1_thread_id, priority);

        return 0;
    }

    return -403;
}
