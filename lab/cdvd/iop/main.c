#include <loadcore.h>
#include <cdvdman.h>
#include <intrman.h>
#include <ioman.h>
#include <stdio.h>
#include <sysmem.h>
#include <thbase.h>
#include <thsemap.h>

#define MODNAME "cdvdtest"
IRX_ID(MODNAME, 1, 1);

// 5-second timeout for sceCdSync(1) polling (IOP sysclock ~36.864 MHz)
#define IOP_SYNC_TIMEOUT_USEC (5 * 1000 * 1000)

static int iop_cdvd_sync(void)
{
    iop_sys_clock_t start, now, limit;
    USec2SysClock(IOP_SYNC_TIMEOUT_USEC, &limit);
    GetSystemTime(&start);
    while (sceCdSync(1)) {
        GetSystemTime(&now);
        if ((now.lo - start.lo) >= limit.lo) {
            printf("  [SYNC_TIMEOUT]\n");
            sceCdStop();
            DelayThread(100 * 1000); // 100ms for stop to settle
            return -1;
        }
        DelayThread(1000); // 1ms poll
    }
    return 0;
}

// Read buffers
#define BUF_SECTORS (32)
#define BUF_SIZE    (BUF_SECTORS * 2048)
static unsigned char g_buf[BUF_SIZE]   __attribute__((aligned(64)));
static unsigned char g_buf2[2048]      __attribute__((aligned(64)));

// IOP-side callback state
static volatile int g_cb_called;
static volatile int g_cb_reason;
static volatile int g_cb_intr_ctx;    // QueryIntrContext() from inside callback
static volatile int g_cb_thread_id;   // GetThreadId() from inside callback
static volatile int g_cb_sync_inside; // sceCdSync(1) from inside callback
static volatile int g_cb_reenter_ret; // sceCdRead() return value from inside callback

static int g_iop_lsn; // LSN for reentry test, set before read

//--------------------------------------------------------------
static void iop_test_callback(int reason)
{
    g_cb_called     = 1;
    g_cb_reason     = reason;
    g_cb_intr_ctx   = QueryIntrContext();
    g_cb_thread_id  = GetThreadId();
    g_cb_sync_inside = sceCdSync(1);

    // Test: can we issue a new read from inside the callback?
    sceCdRMode mode = {
        .trycount    = 0,
        .spindlctrl  = SCECdSpinNom,
        .datapattern = SCECdSecS2048,
        .pad         = 0
    };
    g_cb_reenter_ret = sceCdRead(g_iop_lsn, 1, g_buf2, &mode);
}

//--------------------------------------------------------------
int _start()
{
    printf("[IOP_TESTS]\n");

    // --- Memory ---
    printf("[IOP_MEM]\n");
    printf("  total_free: %u\n", QueryTotalFreeMemSize());
    printf("  max_free: %u\n", QueryMaxFreeMemSize());
    printf("[/IOP_MEM]\n");

    // --- Setup ---
    sceCdRMode mode = {
        .trycount    = 0,
        .spindlctrl  = SCECdSpinNom,
        .datapattern = SCECdSecS2048,
        .pad         = 0
    };
    int init_ret   = sceCdInit(SCECdINIT);
    int disk_type  = sceCdGetDiskType();
    int mmode_ret  = sceCdMmode(SCECdMmodeCd);
    sceCdlFILE fp;
    int search_ret = sceCdSearchFile(&fp, "\\TEST.BIN;1");
    g_iop_lsn = fp.lsn;

    printf("[IOP_SETUP]\n");
    printf("  init_ret: %d err: %d\n", init_ret, sceCdGetError());
    printf("  disk_type: %d mmode_ret: %d err: %d\n", disk_type, mmode_ret, sceCdGetError());
    printf("  search_ret: %d lsn: %d err: %d\n", search_ret, fp.lsn, sceCdGetError());
    printf("[/IOP_SETUP]\n");

    // --- Callback test: sceCdRead (ECS_EXTERNAL) ---
    g_cb_called = 0;
    g_cb_reenter_ret = -99;
    sceCdCallback(iop_test_callback);

    int read_ret  = sceCdRead(fp.lsn, BUF_SECTORS, g_buf, &mode);
    int sync_ret  = iop_cdvd_sync();
    int err_sync  = sceCdGetError();

    // If reentry read started, wait for it too
    if (g_cb_reenter_ret == 1) {
        iop_cdvd_sync();
    }

    sceCdCallback(NULL);

    printf("[IOP_CALLBACK_TEST]\n");
    printf("  [sceCdRead] read_ret: %d sync_ret: %d err: %d\n", read_ret, sync_ret, err_sync);
    printf("  [sceCdRead] cb_called: %d cb_reason: %d\n", g_cb_called, g_cb_reason);
    printf("  [sceCdRead] cb_intr_ctx: %d cb_thread_id: %d\n", g_cb_intr_ctx, g_cb_thread_id);
    printf("  [sceCdRead] cb_sync_inside: %d cb_reenter_ret: %d\n", g_cb_sync_inside, g_cb_reenter_ret);

    // --- Callback test: sceCdSearchFile (ECS_SEARCHFILE) ---
    g_cb_called = 0;
    sceCdCallback(iop_test_callback);

    sceCdlFILE fp2;
    int sf_ret = sceCdSearchFile(&fp2, "\\TEST.BIN;1");
    DelayThread(100 * 1000); // 100ms — SearchFile is sync but allow any async cb

    sceCdCallback(NULL);

    printf("  [sceCdSearchFile] sf_ret: %d cb_called: %d\n", sf_ret, g_cb_called);

    // --- Callback test: ioman cdrom0: open/read/close (ECS_IOOPS) ---
    g_cb_called = 0;
    sceCdCallback(iop_test_callback);

    int fd = open("cdrom0:\\TEST.BIN;1", 1 /* O_RDONLY */);
    int fio_bytes = -1;
    if (fd >= 0) {
        fio_bytes = read(fd, g_buf, 2048);
        close(fd);
    }
    DelayThread(100 * 1000); // 100ms — allow any async cb

    sceCdCallback(NULL);

    printf("  [ioman_fio] fd: %d read_bytes: %d cb_called: %d\n", fd, fio_bytes, g_cb_called);
    printf("[/IOP_CALLBACK_TEST]\n");

    // --- Concurrent reads test ---
    //printf("[IOP_CONCURRENT_TEST]\n");
    //int r1 = sceCdRead(fp.lsn, BUF_SECTORS, g_buf, &mode);
    //int r2 = sceCdRead(fp.lsn, 1, g_buf2, &mode);
    //int err_r2 = sceCdGetError();
    //iop_cdvd_sync();
    //if (r2 == 1) iop_cdvd_sync();
    //int err_after = sceCdGetError();
    //printf("  read1_ret: %d\n", r1);
    //printf("  read2_while_busy_ret: %d err: %d\n", r2, err_r2);
    //printf("  err_after_sync: %d\n", err_after);
    //printf("[/IOP_CONCURRENT_TEST]\n");

    printf("[/IOP_TESTS]\n");

    return MODULE_NO_RESIDENT_END;
}
