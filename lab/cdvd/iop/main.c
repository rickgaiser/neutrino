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

//--------------------------------------------------------------
// Test selection — set to 0 to skip a section
//--------------------------------------------------------------
#define IOP_RUN_MEM      0  // QueryTotalFreeMemSize / QueryMaxFreeMemSize
#define IOP_RUN_CALLBACK 0  // sceCdRead / sceCdSearchFile / ioman callback tests
#define IOP_RUN_SPEED    1  // read throughput: sceCdRead / fioRead / sceCdStRead
#define IOP_RUN_STREAM   0  // parameterized sceCdSt* sweep

// 5-second timeout for sceCdSync(1) polling (IOP sysclock ~36.864 MHz)
#define IOP_SYNC_TIMEOUT_USEC (5 * 1000 * 1000)

//--------------------------------------------------------------
// Classification helpers — compare measured values against the reference table
// collected across all known IOPRP firmware images (RESULTS.md).
// Returns "[OK]"          if the value matches ALL tested IOPRP images.
//         "[matches: …]"  if it matches only a specific firmware subset.
//         "[ERROR]"       if no known IOPRP firmware produces this value.
//--------------------------------------------------------------
static const char *iop_chk_mmode(int v)
{
    if (v == 18) return "[matches: bios,ioprp14-16]";
    if (v == 1)  return "[matches: ioprp165+]";
    return "[ERROR]";
}

static __attribute__((unused)) const char *iop_chk_open(int fd)
{
    if (fd >= 0)   return "[matches: most versions]";
    if (fd == -16) return "[matches: ioprp250,ioprp253,dnas280]";
    if (fd == -1)  return "[matches: ioprp210,ioprp213,ioprp224,ioprp234,ioprp255,ioprp260]";
    return "[ERROR]";
}

static __attribute__((unused)) const char *iop_chk_fio_bytes(int fd, int bytes)
{
    if (fd < 0)        return "[N/A: open failed]";
    if (bytes == 2048) return "[OK]";
    return "[ERROR]";
}

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

// Returns microseconds elapsed since *start (uses lo field; wraps after ~116s)
static u32 iop_elapsed_usec(const iop_sys_clock_t *start)
{
    iop_sys_clock_t now, diff;
    GetSystemTime(&now);
    diff.lo = now.lo - start->lo;
    diff.hi = now.hi - start->hi - (now.lo < start->lo ? 1 : 0);
    u32 sec = 0, usec = 0;
    SysClock2USec(&diff, &sec, &usec);
    return sec * 1000000 + usec;
}

// Read buffers (max bufmax=128 sectors for streaming test)
#define BUF_SECTORS  (128)
#define BUF_SIZE     (BUF_SECTORS * 2048)
#define STREAM_READS (4)
static unsigned char g_buf[BUF_SIZE]    __attribute__((aligned(64)));
static unsigned char g_buf2[64*2048]    __attribute__((aligned(64)));

// IOP-side callback state
static volatile int g_cb_called;
static volatile int g_cb_reason;
static volatile int g_cb_intr_ctx;    // QueryIntrContext() from inside callback
static volatile int g_cb_thread_id;   // GetThreadId() from inside callback
static volatile int g_cb_sync_inside; // sceCdSync(1) from inside callback
static volatile int g_cb_reenter_ret; // sceCdRead() return value from inside callback

static int g_iop_lsn; // LSN for reentry test, set before read

// Streaming callback state (sceCdSt* API)
#define STREAM_CB_MAX 128
static volatile int g_stream_cb_count;
static volatile int g_stream_reasons[STREAM_CB_MAX];

// Streaming parameter sweep: bufmax x bankmax x sectors-per-read
static const int __attribute__((unused)) k_bufmax[]  = {16, 32, 64, 128};
static const int __attribute__((unused)) k_bankmax[] = {2, 3, 4};
static const int __attribute__((unused)) k_sectors[] = {1, 2, 4, 8, 16, 32, 64};
#define N_BUFMAX  4
#define N_BANKMAX 3
#define N_SECTORS 7
// 1.5 MiB/s pacing: µs to wait after reading one sector (2048 B @ 1572864 B/s ≈ 1302 µs)
#define USEC_PER_SECTOR 1302

#define SPEED_RUNS 3

//--------------------------------------------------------------
static void __attribute__((unused)) iop_stream_callback(int reason)
{
    int i = g_stream_cb_count++;
    if (i < STREAM_CB_MAX)
        g_stream_reasons[i] = reason;
}

//--------------------------------------------------------------
static void __attribute__((unused)) iop_test_callback(int reason)
{
    int read_again = g_cb_called == 0; // First time callback is called during a read

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

    if (read_again) {
        g_cb_reenter_ret = sceCdRead(g_iop_lsn, 1, g_buf2, &mode);
    }
}

//--------------------------------------------------------------
int _start()
{
    printf("[IOP_TESTS]\n");

#if IOP_RUN_MEM
    printf("[IOP_MEM]\n");
    printf("  total_free: %u\n", QueryTotalFreeMemSize());
    printf("  max_free: %u\n", QueryMaxFreeMemSize());
    printf("[/IOP_MEM]\n");
#endif

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
    printf("  init_ret: %d %s err: %d\n", init_ret, init_ret == 1 ? "[OK]" : "[ERROR]", sceCdGetError());
    printf("  disk_type: %d mmode_ret: %d %s err: %d\n", disk_type, mmode_ret, iop_chk_mmode(mmode_ret), sceCdGetError());
    printf("  search_ret: %d lsn: %d err: %d\n", search_ret, fp.lsn, sceCdGetError());
    printf("[/IOP_SETUP]\n");

#if IOP_RUN_CALLBACK
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
    printf("  [sceCdRead] read_ret: %d %s sync_ret: %d err: %d\n",
           read_ret, read_ret == 1 ? "[OK]" : "[ERROR]", sync_ret, err_sync);
    printf("  [sceCdRead] cb_called: %d %s cb_reason: %d %s\n",
           g_cb_called, g_cb_called == 1 ? "[OK]" : "[ERROR]",
           g_cb_reason, g_cb_reason == 1 ? "[OK]" : "[ERROR]");
    printf("  [sceCdRead] cb_intr_ctx: %d %s\n",
           g_cb_intr_ctx, g_cb_intr_ctx == 1 ? "[OK]" : "[ERROR]");
    printf("  [sceCdRead] cb_thread_id: %d %s\n",
           g_cb_thread_id, g_cb_thread_id == -100 ? "[OK]" : "[ERROR]");
    printf("  [sceCdRead] cb_sync_inside: %d %s\n",
           g_cb_sync_inside, g_cb_sync_inside == 0 ? "[OK]" : "[ERROR]");
    printf("  [sceCdRead] cb_reenter_ret: %d %s\n",
           g_cb_reenter_ret, g_cb_reenter_ret == 1 ? "[OK]" : "[ERROR]");

    // --- Callback test: sceCdSearchFile (ECS_SEARCHFILE) ---
    g_cb_called = 0;
    sceCdCallback(iop_test_callback);

    sceCdlFILE fp2;
    int sf_ret = sceCdSearchFile(&fp2, "\\TEST.BIN;1");
    DelayThread(100 * 1000); // 100ms — SearchFile is sync but allow any async cb

    sceCdCallback(NULL);

    printf("  [sceCdSearchFile] sf_ret: %d %s cb_called: %d %s\n",
           sf_ret, sf_ret == 1 ? "[OK]" : sf_ret == 0 ? "[matches: ioprp224,ioprp23]" : "[ERROR]",
           g_cb_called, g_cb_called == 0 ? "[OK]" : "[ERROR]");

    // --- Callback test: sceCdSearchFile in subdirectory (forces extra dir sector read) ---
    g_cb_called = 0;
    sceCdCallback(iop_test_callback);

    sceCdlFILE fp3;
    int sf_sub_ret = sceCdSearchFile(&fp3, "\\SUBDIR\\TEST.BIN;1");
    DelayThread(100 * 1000); // 100ms — allow any async cb

    sceCdCallback(NULL);

    printf("  [sceCdSearchFile_subdir] sf_ret: %d %s cb_called: %d %s\n",
           sf_sub_ret, sf_sub_ret == 1 ? "[OK]" : "[ERROR]",
           g_cb_called, g_cb_called == 0 ? "[OK: no cb]" : "[cb fired!]");

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

    printf("  [ioman_fio] fd: %d %s read_bytes: %d %s cb_called: %d\n",
           fd, iop_chk_open(fd), fio_bytes, iop_chk_fio_bytes(fd, fio_bytes), g_cb_called);
    printf("[/IOP_CALLBACK_TEST]\n");
#endif // IOP_RUN_CALLBACK

#if IOP_RUN_SPEED
    // --- Read speed test: sceCdRead, fioRead (large/small), sceCdStRead ---
    printf("[IOP_READ_SPEED]\n");

    // sceCdRead: SPEED_RUNS bulk reads of BUF_SECTORS sectors
    {
        iop_sys_clock_t t0; GetSystemTime(&t0);
        int ok = 1;
        for (int i = 0; i < SPEED_RUNS && ok; i++) {
            if (sceCdRead(fp.lsn, BUF_SECTORS, g_buf, &mode) != 1) { ok = 0; break; }
            if (iop_cdvd_sync() != 0) { ok = 0; break; }
        }
        u32 elapsed = iop_elapsed_usec(&t0);
        u32 total_bytes = (u32)BUF_SECTORS * 2048 * SPEED_RUNS;
        u32 kibps = elapsed > 0 ? (total_bytes / 1024) * 1000000 / elapsed : 0;
        printf("  [sceCdRead]     ok=%d sectors=%d runs=%d kb=%u usec=%u kibps=%u\n",
               ok, BUF_SECTORS, SPEED_RUNS, total_bytes / 1024, elapsed, kibps);
    }

    // fioRead large: one read() call of BUF_SECTORS sectors per run
    {
        iop_sys_clock_t t0; GetSystemTime(&t0);
        u32 total_bytes = 0;
        for (int i = 0; i < SPEED_RUNS; i++) {
            int rfd = open("cdrom0:\\TEST.BIN;1", 1 /* O_RDONLY */);
            if (rfd >= 0) {
                int n = read(rfd, g_buf, BUF_SECTORS * 2048);
                if (n > 0) total_bytes += (u32)n;
                close(rfd);
            }
        }
        u32 elapsed = iop_elapsed_usec(&t0);
        u32 kibps = elapsed > 0 ? (total_bytes / 1024) * 1000000 / elapsed : 0;
        printf("  [fioRead_large] runs=%d kb=%u usec=%u kibps=%u\n",
               SPEED_RUNS, total_bytes / 1024, elapsed, kibps);
    }

    // fioRead small: 2048-byte reads, BUF_SECTORS iterations in one open
    {
        iop_sys_clock_t t0; GetSystemTime(&t0);
        u32 total_bytes = 0;
        int rfd = open("cdrom0:\\TEST.BIN;1", 1 /* O_RDONLY */);
        if (rfd >= 0) {
            for (int i = 0; i < BUF_SECTORS; i++) {
                int n = read(rfd, g_buf, 2048);
                if (n <= 0) break;
                total_bytes += (u32)n;
            }
            close(rfd);
        }
        u32 elapsed = iop_elapsed_usec(&t0);
        u32 kibps = elapsed > 0 ? (total_bytes / 1024) * 1000000 / elapsed : 0;
        printf("  [fioRead_small] sector_reads=%d kb=%u usec=%u kibps=%u\n",
               BUF_SECTORS, total_bytes / 1024, elapsed, kibps);
    }

    // sceCdStRead: streaming (g_buf as ring buffer, g_buf2 as read dest)
    // 4 banks x 16 sectors = 64 sector ring; one bank per STMBLK read; stream BUF_SECTORS total
    {
        int stbufmax      = 64;          // 4 banks x 16 sectors
        int bankmax       = 4;
        int nsec          = 16;          // one bank per read
        int total_sectors = BUF_SECTORS; // 256KiB
        int init_ret  = sceCdStInit(stbufmax, bankmax, g_buf);
        int start_ret = sceCdStStart(fp.lsn, &mode);
        printf("  [sceCdStRead]   init_ret=%d start_ret=%d\n", init_ret, start_ret);
        if (init_ret != 1 || start_ret != 1) {
            sceCdStStop();
            printf("  [sceCdStRead]   init/start failed, skipping\n");
        } else {
            DelayThread(200 * 1000); // 200ms pre-fill

            u32 target_bytes = (u32)total_sectors * 2048;
            iop_sys_clock_t t0; GetSystemTime(&t0);
            u32 total_bytes = 0;
            int ok = 1;
            while (total_bytes < target_bytes && ok) {
                u32 st_err = 0;
                // STMBLK (1): blocks until nsec sectors are read or error
                int rd = sceCdStRead(nsec, (u32 *)g_buf2, 1, &st_err);
                if (rd <= 0) { ok = 0; break; }
                total_bytes += (u32)rd * 2048;
            }
            u32 elapsed = iop_elapsed_usec(&t0);
            sceCdStStop();

            u32 kibps = elapsed > 0 ? (total_bytes / 1024) * 1000000 / elapsed : 0;
            printf("  [sceCdStRead]   ok=%d stbufmax=%d bankmax=%d nsec=%d kb=%u usec=%u kibps=%u\n",
                   ok, stbufmax, bankmax, nsec, total_bytes / 1024, elapsed, kibps);
        } // else init/start succeeded
    }

    printf("[/IOP_READ_SPEED]\n");
#endif // IOP_RUN_SPEED

#if IOP_RUN_STREAM
    // --- Parameterized streaming test (sceCdSt* API) ---
    // Sweeps bufmax {16,32,64,128} x bankmax {2,3,4} x sectors-per-read {1..64, <=bufmax}
    // paced at ~1.5 MiB/s between reads (STREAM_READS reads per combination).
    sceCdCallback(iop_stream_callback);
    printf("[IOP_STREAM_PARAM_TEST]\n");

    for (int bi = 0; bi < N_BUFMAX; bi++) {
        int bufmax = k_bufmax[bi];
        for (int ki = 0; ki < N_BANKMAX; ki++) {
            int bankmax = k_bankmax[ki];
            int init_ret = sceCdStInit(bufmax, bankmax, g_buf);

            for (int si = 0; si < N_SECTORS; si++) {
                int nsec = k_sectors[si];
                if (nsec > bufmax) continue;

                g_stream_cb_count = 0;
                int start_ret = sceCdStStart(fp.lsn, &mode);
                DelayThread(200 * 1000); // 200ms — allow ring buffer to pre-fill

                int stream_ok = 1;
                u32 st_err = 0;
                for (int r = 0; r < STREAM_READS; r++) {
                    int rd = sceCdStRead(nsec, (u32 *)g_buf2, 0, &st_err);
                    if (rd <= 0) { stream_ok = 0; break; }
                    DelayThread(nsec * USEC_PER_SECTOR); // pace at ~1.5 MiB/s
                }

                int stop_ret = sceCdStStop();

                int n = g_stream_cb_count;
                if (n > STREAM_CB_MAX) n = STREAM_CB_MAX;
                printf("  [B=%d K=%d S=%d] init=%d start=%d stop=%d ok=%d err=%u cb=%d r:",
                       bufmax, bankmax, nsec, init_ret, start_ret, stop_ret,
                       stream_ok, st_err, g_stream_cb_count);
                for (int i = 0; i < n; i++) printf(" %d", g_stream_reasons[i]);
                printf("\n");
            }
        }
    }

    sceCdCallback(NULL);
    printf("[/IOP_STREAM_PARAM_TEST]\n");
#endif // IOP_RUN_STREAM

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
