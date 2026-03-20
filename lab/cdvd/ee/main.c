/*
 * CDVD Behavior Test Suite
 *
 * Tests CDVD behavior across many IOPRP firmware versions to identify
 * differences that can guide neutrino emulation improvements.
 *
 * How to use:
 * - make iso sim       (builds cdvd.iso and launches PCSX2)
 * - EE serial output is visible in PCSX2's EE Console
 * - IOP TTY output is visible in PCSX2's IOP Console (separate window)
 * - Save both outputs as results/run_<date>_ee.txt and results/run_<date>_iop.txt
 *
 * The test ISO (cdvd.iso) contains:
 * - CDVD.ELF     — this executable (booted via SYSTEM.CNF)
 * - CDVD.IRX     — IOP test module (loaded at runtime per IOPRP version)
 * - IRP*.IMG     — IOPRP firmware images (loaded from disc, not embedded)
 * - TEST.BIN     — 512KB test data file for read tests
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
#include <sbv_patches.h>
#include <libcdvd-common.h>
#include <timer.h>
#include <ps2sdkapi.h>

// libc
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

void _ps2sdk_timezone_update() {}

DISABLE_PATCHED_FUNCTIONS();

//--------------------------------------------------------------
// Test selection — set to 0 to skip a section
//--------------------------------------------------------------
#define RUN_MODULES    0  // module list + IOP free memory
#define RUN_CALLBACK   0  // sceCdRead callback timing
#define RUN_REENTER    0  // sceCdRead re-entry from callback
#define RUN_SYNC       0  // sceCdSync(0) vs poll, no-sync byte test
#define RUN_FIO        0  // cdrom0: fioRead, callback interaction
#define RUN_CONCURRENT 0  // second sceCdRead while busy
#define RUN_SPEED      1  // read throughput: sceCdRead / fioRead / sceCdStRead
#define RUN_IOP_MODULE 1  // load and run CDVD.IRX

//--------------------------------------------------------------
// Classification helpers — compare measured values against the reference table
// collected across all known IOPRP firmware images (RESULTS.md).
// Returns "[OK]"          if the value matches ALL tested IOPRP images.
//         "[matches: …]"  if it matches only a specific firmware subset.
//         "[ERROR]"       if no known IOPRP firmware produces this value.
//--------------------------------------------------------------
static const char *ee_chk_mmode(int v)
{
    if (v == 0) return "[matches: bios,ioprp14-16]";
    if (v == 1) return "[matches: ioprp165+]";
    return "[ERROR]";
}

//--------------------------------------------------------------
// IOPRP table — each entry maps a test label to the ISO9660 path
// of the corresponding firmware image on the test disc.
// iop_path: passed directly to SifIopReset() — the IOP reads from disc itself.
// Naming: "IRP*.IMG" for ioprp*, "DNAS*.IMG"/"DNS*.IMG" for dnas*.
// All names are ≤8.3 characters (ISO9660 level 1 limit).
//--------------------------------------------------------------
typedef struct {
    const char *name;     // label printed in output (e.g., "ioprp14")
    const char *iop_path; // SifIopReset path (e.g., "cdrom0:\\MODULES\\IRP14.IMG;1")
} ioprp_entry_t;

static const ioprp_entry_t g_ioprp_table[] = {
    { "ioprp14",    "rom0:UDNL cdrom0:\\MODULES\\IRP14.IMG;1"   },
    { "ioprp15",    "rom0:UDNL cdrom0:\\MODULES\\IRP15.IMG;1"   },
    { "ioprp16",    "rom0:UDNL cdrom0:\\MODULES\\IRP16.IMG;1"   },
    { "ioprp165",   "rom0:UDNL cdrom0:\\MODULES\\IRP165.IMG;1"  },
    { "ioprp202",   "rom0:UDNL cdrom0:\\MODULES\\IRP202.IMG;1"  },
    { "ioprp205",   "rom0:UDNL cdrom0:\\MODULES\\IRP205.IMG;1"  },
    { "ioprp21",    "rom0:UDNL cdrom0:\\MODULES\\IRP21.IMG;1"   },
    { "ioprp210",   "rom0:UDNL cdrom0:\\MODULES\\IRP210.IMG;1"  },
    { "ioprp211",   "rom0:UDNL cdrom0:\\MODULES\\IRP211.IMG;1"  },
    { "ioprp213",   "rom0:UDNL cdrom0:\\MODULES\\IRP213.IMG;1"  },
    { "ioprp214",   "rom0:UDNL cdrom0:\\MODULES\\IRP214.IMG;1"  },
    { "ioprp224",   "rom0:UDNL cdrom0:\\MODULES\\IRP224.IMG;1"  },
    { "ioprp23",    "rom0:UDNL cdrom0:\\MODULES\\IRP23.IMG;1"   },
    { "ioprp234",   "rom0:UDNL cdrom0:\\MODULES\\IRP234.IMG;1"  },
    { "ioprp241",   "rom0:UDNL cdrom0:\\MODULES\\IRP241.IMG;1"  },
    { "ioprp242",   "rom0:UDNL cdrom0:\\MODULES\\IRP242.IMG;1"  },
    { "ioprp243",   "rom0:UDNL cdrom0:\\MODULES\\IRP243.IMG;1"  },
    { "ioprp250",   "rom0:UDNL cdrom0:\\MODULES\\IRP250.IMG;1"  },
    { "ioprp253",   "rom0:UDNL cdrom0:\\MODULES\\IRP253.IMG;1"  },
    { "ioprp255",   "rom0:UDNL cdrom0:\\MODULES\\IRP255.IMG;1"  },
    { "ioprp260",   "rom0:UDNL cdrom0:\\MODULES\\IRP260.IMG;1"  },
    { "ioprp271",   "rom0:UDNL cdrom0:\\MODULES\\IRP271.IMG;1"  },
    { "ioprp271_2", "rom0:UDNL cdrom0:\\MODULES\\IRP2712.IMG;1" },
    { "ioprp280",   "rom0:UDNL cdrom0:\\MODULES\\IRP280.IMG;1"  },
    { "ioprp300",   "rom0:UDNL cdrom0:\\MODULES\\IRP300.IMG;1"  },
    { "ioprp300_2", "rom0:UDNL cdrom0:\\MODULES\\IRP3002.IMG;1" },
    { "ioprp300_3", "rom0:UDNL cdrom0:\\MODULES\\IRP3003.IMG;1" },
    { "ioprp300_4", "rom0:UDNL cdrom0:\\MODULES\\IRP3004.IMG;1" },
    { "ioprp310",   "rom0:UDNL cdrom0:\\MODULES\\IRP310.IMG;1"  },
    { "dnas280",    "rom0:UDNL cdrom0:\\MODULES\\DNAS280.IMG;1" },
    { "dnas300",    "rom0:UDNL cdrom0:\\MODULES\\DNAS300.IMG;1" },
    { "dnas300_2",  "rom0:UDNL cdrom0:\\MODULES\\DNS3002.IMG;1" },
    { NULL, NULL }
};

//--------------------------------------------------------------
// sceCdSync with a 5-second timeout. Returns 0 on completion, -1 on timeout.
// Use this everywhere instead of sceCdSync(0) so the test never hangs.
//--------------------------------------------------------------
#define SYNC_TIMEOUT_TICKS (PS2_CLOCKS_PER_SEC * 5)  // 5s

static int cdvd_sync(void)
{
    struct timespec tv = {0};
    tv.tv_sec = 0;
    tv.tv_nsec = 1000000; // 1ms — short enough not to skew speed measurements

    ps2_clock_t end = ps2_clock() + SYNC_TIMEOUT_TICKS;
    while (sceCdSync(1)) {
        nanosleep(&tv, NULL);
        if (ps2_clock() > end) {
            _print("  [SYNC_TIMEOUT]\n");
            // Abort the pending operation so cdvdman is ready for next call
            sceCdStop();
            // Give stop command time to propagate (brief spin)
            for (volatile int z = 0; z < 1000000; z++) {}
            return -1;
        }
    }
    return 0;
}

//--------------------------------------------------------------
// Read buffers
//--------------------------------------------------------------
#define BUF_SECTORS (128)
#define BUF_SIZE    (BUF_SECTORS * 2048)
static unsigned char g_buf[BUF_SIZE]  __attribute__((aligned(64)));
static unsigned char g_buf2[BUF_SIZE] __attribute__((aligned(64)));

#define SPEED_RUNS 3

// Callback thread stack — created once, persists across IOP resets
#define CB_STACK_SIZE (16 * 1024)
static unsigned char cb_stack[CB_STACK_SIZE] __attribute__((aligned(16)));

// Per-section callback state
static volatile int g_cb_called;
static volatile int g_cb_reason;
static volatile ps2_clock_t g_cb_tick;
static volatile int g_cb_sync_inside; // sceCdSync(1) return value inside callback
static volatile int g_reenter_ret;    // sceCdRead() return value inside callback
static u32          g_fp_lsn;        // target LSN, readable from callback

//--------------------------------------------------------------
// Section 1+2: Module list + IOP free memory estimate
//--------------------------------------------------------------
static void __attribute__((unused)) section_modules(void)
{
    smod_mod_info_t info;
    smod_mod_info_t *curr = NULL;
    char sName[21];
    u32 txtsz_total = 0;
    u32 dtasz_total = 0;
    u32 bsssz_total = 0;
    u32 top_used = 0;

    _print("[MODULES]\n");
    _print("  txtst   | txtsz   | dtasz   | bsssz   | ver    | name\n");
    _print("  ------------------------------------------------------------\n");
    while (smod_get_next_mod(curr, &info) != 0) {
        smem_read(info.name, sName, 20);
        sName[20] = 0;
        txtsz_total += info.text_size;
        dtasz_total += info.data_size;
        bsssz_total += info.bss_size;

        u32 mod_end = info.text_start + info.text_size + info.data_size + info.bss_size;
        if (mod_end > top_used)
            top_used = mod_end;

        _print("  0x%05x | %6db | %6db | %6db | 0x%04x | %s\n",
               info.text_start, info.text_size, info.data_size, info.bss_size,
               info.version, sName);
        curr = &info;
    }

    u32 total = txtsz_total + dtasz_total + bsssz_total;
    _print("  ------------------------------------------------------------\n");
    _print("  txt_total: %db  dta_total: %db  bss_total: %db\n",
           txtsz_total, dtasz_total, bsssz_total);
    _print("  grand_total: %db (%dKiB)\n", total, total / 1024);

    // IOP RAM = 2MB; estimate free region after last module
    u32 free_start = (top_used + 0xFF) & ~0xFF;
    u32 iop_ram    = 0x200000;
    u32 free_size  = (free_start < iop_ram) ? (iop_ram - free_start) : 0;
    _print("  [FREE_MEM_ESTIMATE] top_used=0x%05x free_start=0x%05x"
           " free_size=%db (%dKiB)\n",
           top_used, free_start, free_size, free_size / 1024);
    _print("[/MODULES]\n");
}

//--------------------------------------------------------------
// Section setup: init CDVD and find test file
// Returns 1 on success (file found), 0 on failure
//--------------------------------------------------------------
static int section_setup(sceCdlFILE *fp_out)
{
    int init_ret   = sceCdInit(SCECdINIT);
    int disk_type  = sceCdGetDiskType();
    int err_dt     = sceCdGetError();
    int mmode_ret  = sceCdMmode(SCECdMmodeCd);
    int err_mm     = sceCdGetError();
    int search_ret = sceCdSearchFile(fp_out, "\\TEST.BIN;1");
    int err_sf     = sceCdGetError();

    _print("[SETUP]\n");
    _print("  init_ret: %d %s err: %d\n", init_ret, init_ret == 1 ? "[OK]" : "[ERROR]", sceCdGetError());
    _print("  disk_type: %d %s err: %d\n", disk_type, disk_type == 18 ? "[OK]" : "[ERROR]", err_dt);
    _print("  mmode_ret: %d %s err: %d\n", mmode_ret, ee_chk_mmode(mmode_ret), err_mm);
    _print("  search_ret: %d lsn: %d size: %d err: %d\n",
           search_ret, fp_out->lsn, fp_out->size, err_sf);
    _print("[/SETUP]\n");

    return (search_ret != 0);
}

//--------------------------------------------------------------
// Section 3: Callback timing — when relative to sceCdRead/sceCdSync?
//--------------------------------------------------------------
static void cb_timing(int reason)
{
    g_cb_called     = 1;
    g_cb_reason     = reason;
    g_cb_tick       = ps2_clock();
    g_cb_sync_inside = sceCdSync(1); // 0=done, 1=still busy
}

static void __attribute__((unused)) section_callback(const sceCdlFILE *fp)
{
    sceCdRMode mode = { .trycount=0, .spindlctrl=SCECdSpinNom,
                        .datapattern=SCECdSecS2048, .pad=0 };

    g_cb_called = 0;
    g_cb_reason = -1;
    g_cb_sync_inside = -1;
    sceCdCallback(cb_timing);

    ps2_clock_t t_before_read = ps2_clock();
    int read_ret = sceCdRead(fp->lsn, BUF_SECTORS, g_buf, &mode);
    ps2_clock_t t_after_read = ps2_clock();

    // Poll ~100ms to see if callback fires before we call sceCdSync
    int cb_before_sync = 0;
    ps2_clock_t poll_end = ps2_clock() + PS2_CLOCKS_PER_MSEC * 100;
    while (ps2_clock() < poll_end) {
        if (g_cb_called) { cb_before_sync = 1; break; }
    }
    ps2_clock_t t_cb_if_early = g_cb_tick;

    int sync_ret = cdvd_sync();
    ps2_clock_t t_after_sync = ps2_clock();

    sceCdCallback(NULL);

    _print("[CALLBACK_TEST]\n");
    _print("  read_ret: %d %s\n", read_ret, read_ret == 1 ? "[OK]" : "[ERROR]");
    _print("  cb_before_sync_100ms_poll: %d %s\n", cb_before_sync,
           cb_before_sync == 0 ? "[OK]" : "[ERROR]");
    _print("  cb_after_sync: %d %s\n", g_cb_called,
           g_cb_called == 1 ? "[OK]" : "[ERROR]");
    _print("  cb_reason: %d %s\n", g_cb_reason,
           g_cb_reason == 1 ? "[OK]" : "[ERROR]");
    _print("  cb_sync_state_inside: %d %s\n", g_cb_sync_inside,
           g_cb_sync_inside == 0 ? "[OK]" : "[ERROR]");
    _print("  sync_ret: %d %s\n", sync_ret, sync_ret == 0 ? "[OK]" : "[ERROR]");
    _print("  ticks_read_to_sync: %llu\n", t_after_sync - t_after_read);
    if (cb_before_sync)
        _print("  ticks_read_to_cb: %llu\n", t_cb_if_early - t_after_read);
    else if (g_cb_called)
        _print("  ticks_read_to_cb: %llu\n", g_cb_tick - t_after_read);
    (void)t_before_read;
    _print("[/CALLBACK_TEST]\n");
}

//--------------------------------------------------------------
// Section 4: Reentrancy — can sceCdRead() be called from inside callback?
//--------------------------------------------------------------
static void cb_reenter(int reason)
{
    g_cb_called = 1;
    g_cb_reason = reason;
    sceCdRMode mode = { .trycount=0, .spindlctrl=SCECdSpinNom,
                        .datapattern=SCECdSecS2048, .pad=0 };
    g_reenter_ret = sceCdRead(g_fp_lsn, 1, g_buf2, &mode);
}

static void __attribute__((unused)) section_reenter(const sceCdlFILE *fp)
{
    sceCdRMode mode = { .trycount=0, .spindlctrl=SCECdSpinNom,
                        .datapattern=SCECdSecS2048, .pad=0 };

    g_cb_called   = 0;
    g_reenter_ret = -99;
    g_fp_lsn      = fp->lsn;
    sceCdCallback(cb_reenter);

    int read1_ret = sceCdRead(fp->lsn, BUF_SECTORS, g_buf, &mode);
    cdvd_sync(); // wait for first read (and lets callback fire)
    // If reentry read was started inside callback, wait for it too
    if (g_reenter_ret == 1)
        cdvd_sync();

    sceCdCallback(NULL);

    _print("[REENTER_TEST]\n");
    _print("  read1_ret: %d %s\n", read1_ret, read1_ret == 1 ? "[OK]" : "[ERROR]");
    _print("  cb_called: %d cb_reason: %d\n", g_cb_called, g_cb_reason);
    _print("  reenter_from_callback_ret: %d %s\n", g_reenter_ret,
           g_reenter_ret == 1 ? "[OK]" : "[ERROR]");
    _print("[/REENTER_TEST]\n");
}

//--------------------------------------------------------------
// Section 5: Sync mode timing — sceCdSync(0) vs sceCdSync(1) poll
//--------------------------------------------------------------
static void __attribute__((unused)) section_sync_modes(const sceCdlFILE *fp)
{
    sceCdRMode mode = { .trycount=0, .spindlctrl=SCECdSpinNom,
                        .datapattern=SCECdSecS2048, .pad=0 };
    sceCdCallback(NULL);

    _print("[SYNC_TEST]\n");

    // 5a: sceCdSync via cdvd_sync() (polls with timeout)
    sceCdRead(fp->lsn, BUF_SECTORS, g_buf, &mode);
    ps2_clock_t t0 = ps2_clock();
    int sync0_ret = cdvd_sync();
    ps2_clock_t t1 = ps2_clock();
    _print("  sync0_ret: %d ticks: %llu\n", sync0_ret, t1 - t0);

    // 5b: sceCdSync(1) polling (raw, counts iterations)
    sceCdRead(fp->lsn, BUF_SECTORS, g_buf, &mode);
    t0 = ps2_clock();
    u32 poll_count = 0;
    ps2_clock_t sync1_end = ps2_clock() + SYNC_TIMEOUT_TICKS;
    while (sceCdSync(1)) {
        poll_count++;
        if (ps2_clock() > sync1_end) { poll_count = 0; break; }
    }
    t1 = ps2_clock();
    _print("  sync1_poll_count: %u ticks: %llu\n", poll_count, t1 - t0);

    // 5c: No sync — check if buffer has data before sync
    memset(g_buf, 0xAA, 2048);
    sceCdRead(fp->lsn, 1, g_buf, &mode);
    // Read byte immediately without any sync
    unsigned char byte_immediate = *((volatile unsigned char *)g_buf);
    // Spin briefly (~1M iterations) without calling sceCdSync
    for (volatile int z = 0; z < 1000000; z++) {}
    unsigned char byte_after_spin = *((volatile unsigned char *)g_buf);
    cdvd_sync();
    unsigned char byte_after_sync = *((volatile unsigned char *)g_buf);
    _print("  nosync_byte0_immediate: 0x%02x %s\n", byte_immediate,
           byte_immediate == 0xaa ? "[OK]" : "[ERROR]");
    _print("  nosync_byte0_after_spin: 0x%02x %s\n", byte_after_spin,
           byte_after_spin == 0x31 ? "[OK]" : "[ERROR]");
    _print("  nosync_byte0_after_sync: 0x%02x %s\n", byte_after_sync,
           byte_after_sync == 0x31 ? "[OK]" : "[ERROR]");

    _print("[/SYNC_TEST]\n");
}

//--------------------------------------------------------------
// Section 6: Does cdrom0: file I/O trigger the sceCdRead callback?
//--------------------------------------------------------------
static void cb_fio(int reason)
{
    g_cb_called = 1;
    g_cb_reason = reason;
}

static void __attribute__((unused)) section_fio(void)
{
    g_cb_called = 0;
    g_cb_reason = -1;
    sceCdCallback(cb_fio);

    int fd = open("cdrom0:\\TEST.BIN;1", O_RDONLY);
    int bytes = -1;
    if (fd >= 0) {
        bytes = (int)read(fd, g_buf, 2048);
        close(fd);
    }

    // Brief spin to let callback fire if it's going to
    for (volatile int z = 0; z < 5000000; z++) {}

    sceCdCallback(NULL);

    _print("[FIO_TEST]\n");
    _print("  open_fd: %d\n", fd);
    _print("  read_bytes: %d\n", bytes);
    _print("  cb_called_by_fio: %d\n", g_cb_called);
    _print("  cb_reason: %d\n", g_cb_reason);
    _print("[/FIO_TEST]\n");
}

//--------------------------------------------------------------
// Section 7: Concurrent reads — what happens when 2nd read issued while busy?
//--------------------------------------------------------------
static void __attribute__((unused)) section_concurrent(const sceCdlFILE *fp)
{
    sceCdRMode mode = { .trycount=0, .spindlctrl=SCECdSpinNom,
                        .datapattern=SCECdSecS2048, .pad=0 };
    sceCdCallback(NULL);

    int r1   = sceCdRead(fp->lsn, BUF_SECTORS, g_buf, &mode);
    int r2   = sceCdRead(fp->lsn, 1, g_buf2, &mode);
    int err2 = sceCdGetError();
    cdvd_sync();
    if (r2 == 1) cdvd_sync(); // in case 2nd read also started
    int err_after = sceCdGetError();

    _print("[CONCURRENT_TEST]\n");
    _print("  read1_ret: %d %s\n", r1, r1 == 1 ? "[OK]" : "[ERROR]");
    _print("  read2_while_busy_ret: %d %s err: %d\n", r2,
           r2 == 0 ? "[OK]" : "[ERROR]", err2);
    _print("  err_after_sync: %d %s\n", err_after,
           err_after == 0 ? "[OK]" : "[ERROR]");
    _print("[/CONCURRENT_TEST]\n");
}

//--------------------------------------------------------------
// Section: Read speed — throughput via sceCdRead, fioRead, sceCdStRead
//--------------------------------------------------------------
static void __attribute__((unused)) section_read_speed(const sceCdlFILE *fp)
{
    sceCdRMode mode = { .trycount=0, .spindlctrl=SCECdSpinNom,
                        .datapattern=SCECdSecS2048, .pad=0 };
    sceCdCallback(NULL);

    _print("[READ_SPEED]\n");

    // --- sceCdRead: SPEED_RUNS bulk reads of BUF_SECTORS sectors ---
    {
        ps2_clock_t t0 = ps2_clock();
        int ok = 1;
        for (int i = 0; i < SPEED_RUNS && ok; i++) {
            if (sceCdRead(fp->lsn, BUF_SECTORS, g_buf, &mode) != 1) { ok = 0; break; }
            if (cdvd_sync() != 0) { ok = 0; break; }
        }
        ps2_clock_t elapsed = ps2_clock() - t0;
        u32 total_bytes = (u32)BUF_SECTORS * 2048 * SPEED_RUNS;
        u32 kibps = elapsed > 0 ? (u32)((ps2_clock_t)(total_bytes / 1024) * PS2_CLOCKS_PER_SEC / elapsed) : 0;
        _print("  [sceCdRead]     ok=%d sectors=%d runs=%d kb=%u ticks=%llu kibps=%u\n",
               ok, BUF_SECTORS, SPEED_RUNS, total_bytes / 1024, elapsed, kibps);
    }

    // --- fioRead large: one read() call of BUF_SECTORS sectors per run ---
    {
        ps2_clock_t t0 = ps2_clock();
        u32 total_bytes = 0;
        for (int i = 0; i < SPEED_RUNS; i++) {
            int fd = open("cdrom0:\\TEST.BIN;1", O_RDONLY);
            if (fd >= 0) {
                int n = (int)read(fd, g_buf, BUF_SECTORS * 2048);
                if (n > 0) total_bytes += (u32)n;
                close(fd);
            }
        }
        ps2_clock_t elapsed = ps2_clock() - t0;
        u32 kibps = elapsed > 0 ? (u32)((ps2_clock_t)(total_bytes / 1024) * PS2_CLOCKS_PER_SEC / elapsed) : 0;
        _print("  [fioRead_large] runs=%d kb=%u ticks=%llu kibps=%u\n",
               SPEED_RUNS, total_bytes / 1024, elapsed, kibps);
    }

    // --- fioRead small: 2048-byte reads, BUF_SECTORS iterations in one open ---
    {
        ps2_clock_t t0 = ps2_clock();
        u32 total_bytes = 0;
        int fd = open("cdrom0:\\TEST.BIN;1", O_RDONLY);
        if (fd >= 0) {
            for (int i = 0; i < BUF_SECTORS; i++) {
                int n = (int)read(fd, g_buf, 2048);
                if (n <= 0) break;
                total_bytes += (u32)n;
            }
            close(fd);
        }
        ps2_clock_t elapsed = ps2_clock() - t0;
        u32 kibps = elapsed > 0 ? (u32)((ps2_clock_t)(total_bytes / 1024) * PS2_CLOCKS_PER_SEC / elapsed) : 0;
        _print("  [fioRead_small] sector_reads=%d kb=%u ticks=%llu kibps=%u\n",
               BUF_SECTORS, total_bytes / 1024, elapsed, kibps);
    }

    // --- sceCdStRead: streaming (ring buffer on IOP heap) ---
    // 4 banks x 16 sectors = 64 sector ring; one bank per STMBLK read; stream BUF_SECTORS total
    {
        int stbufmax      = 64;          // 4 banks x 16 sectors
        int bankmax       = 4;
        int nsec          = 16;          // one bank per read
        int total_sectors = BUF_SECTORS; // 256KiB
        void *iop_buf = SifAllocIopHeap((u32)stbufmax * 2048);
        if (!iop_buf) {
            _print("  [sceCdStRead]   iop_alloc_failed\n");
        } else {
            int init_ret  = sceCdStInit((u32)stbufmax, (u32)bankmax, iop_buf);
            int start_ret = sceCdStStart(fp->lsn, &mode);
            _print("  [sceCdStRead]   init_ret=%d start_ret=%d\n", init_ret, start_ret);
            if (init_ret != 1 || start_ret != 1) {
                sceCdStStop();
                SifFreeIopHeap(iop_buf);
                _print("  [sceCdStRead]   init/start failed, skipping\n");
            } else {
                // Pre-fill ring buffer (~200ms)
                struct timespec tv200 = { .tv_sec = 0, .tv_nsec = 200000000 };
                nanosleep(&tv200, NULL);

                u32 target_bytes = (u32)total_sectors * 2048;
                ps2_clock_t t0 = ps2_clock();
                u32 total_bytes = 0;
                int ok = 1;
                while (total_bytes < target_bytes && ok) {
                    u32 st_err = 0;
                    // STMBLK (1): blocks until nsec sectors are read or error
                    int rd = sceCdStRead(nsec, (u32 *)g_buf2, 1, &st_err);
                    if (rd <= 0) { ok = 0; break; }
                    total_bytes += (u32)rd * 2048;
                }
                ps2_clock_t elapsed = ps2_clock() - t0;
                sceCdStStop();
                SifFreeIopHeap(iop_buf);

                u32 kibps = elapsed > 0 ? (u32)((ps2_clock_t)(total_bytes / 1024) * PS2_CLOCKS_PER_SEC / elapsed) : 0;
                _print("  [sceCdStRead]   ok=%d stbufmax=%d bankmax=%d nsec=%d kb=%u ticks=%llu kibps=%u\n",
                       ok, stbufmax, bankmax, nsec, total_bytes / 1024, elapsed, kibps);
            } // else init/start succeeded
        } // else iop_buf allocated
    }

    _print("[/READ_SPEED]\n");
}

//--------------------------------------------------------------
// Run all test sections for one IOPRP version.
// iop_path: cdrom0: path for SifIopReset (e.g. "cdrom0:\\MODULES\\IRP14.IMG;1"),
//           or NULL for plain BIOS baseline.
// is_subsequent: 1 if SifLoadFileExit/ExitIopHeap need to be called first.
//--------------------------------------------------------------
static void run_tests(const char *label, const char *iop_path)
{
    _print("[IOPRP_BEGIN: %s]\n", label);

    #if 0
    // Reset to plain BIOS first so cdvdman starts in a clean state.
    SifExitIopHeap();
    SifLoadFileExit();
    SifExitRpc();
    SifInitRpc(0);
    while(!SifIopReset("", 0)){};
    while(!SifIopSync()) {};
    SifInitRpc(0);
    SifInitIopHeap();
    SifLoadFileInit();
    sceCdInit(SCECdINIT);
    sceCdMmode(SCECdMmodeCd);
    #endif

    if (iop_path != NULL) {
        SifExitIopHeap();
        SifLoadFileExit();
        SifExitRpc();
        SifInitRpc(0);
        while (!SifIopReset(iop_path, 0)) ;
        while (!SifIopSync()) ;
        SifInitRpc(0);
        SifInitIopHeap();
        SifLoadFileInit();
        //sceCdInit(SCECdINIT);
        //sceCdMmode(SCECdMmodeCd);
        // NOTE: do NOT call sceCdInitEeCB here — callback thread was created once in main()
        //       and persists across all IOP resets.
    }

#if RUN_MODULES
    section_modules();
#endif

    sceCdlFILE fp;
    memset(&fp, 0, sizeof(fp));
    if (!section_setup(&fp)) {
        _print("[SKIP] TEST.BIN not found, skipping read tests\n");
        goto load_iop_module;
    }

#if RUN_CALLBACK
    section_callback(&fp);
#endif
#if RUN_REENTER
    section_reenter(&fp);
#endif
#if RUN_SYNC
    section_sync_modes(&fp);
#endif
#if RUN_FIO
    section_fio();
#endif
#if RUN_CONCURRENT
    section_concurrent(&fp);
#endif
#if RUN_SPEED
    section_read_speed(&fp);
#endif

load_iop_module:
#if RUN_IOP_MODULE
    // Load IOP test module from disc — no bin2c needed, disc is always available
    _print("[IOP_MODULE]\n");
    int rv = SifLoadModule("cdrom0:\\CDVD.IRX;1", 0, NULL);
    _print("  SifLoadModule_ret: %d\n", rv);
    // Give IOP module time to execute and print its results to IOP TTY
    // ~50M EE cycles at 294MHz ≈ ~170ms
    for (volatile int z = 0; z < 50000000; z++) {}
    _print("[/IOP_MODULE]\n");
#endif

    _print("[IOPRP_END: %s]\n\n", label);
}

//--------------------------------------------------------------
int main()
{
    SifInitRpc(0);
    SifLoadFileInit();
    SifInitIopHeap();

    // Create the EE-side CDVD callback thread ONCE.
    // It persists across all subsequent IOP resets.
    sceCdInitEeCB(20, cb_stack, CB_STACK_SIZE);

    _print("\n\n[TEST_SUITE_BEGIN]\n");

    // Baseline: plain BIOS (no custom IOPRP)
    run_tests("bios_baseline", NULL);

    // Each IOPRP version — SifIopReset loads directly from disc
    for (int i = 0; g_ioprp_table[i].name != NULL; i++)
        run_tests(g_ioprp_table[i].name, g_ioprp_table[i].iop_path);

    _print("[TEST_SUITE_END]\n");
    _print("All tests done. Frozen.\n");
    while (1) {}

    return 0;
}
