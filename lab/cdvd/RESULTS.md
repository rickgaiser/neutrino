# CDVD Behavior Test Results

## Runs

| Run | Date | Platform | Disc | Status |
|-----|------|----------|------|--------|
| Run 1 | 2026-03-15 | PCSX2 v2.5.336 | None | IOP-side data only; read tests skipped |
| Run 2 | 2026-03-16 | PCSX2 v2.5.336 | Custom test ISO (CD, 512 KB TEST.BIN) | Full EE tests; IOP module disabled |
| Run 3 | 2026-03-16 | PCSX2 v2.5.336 | Custom test ISO (CD, 512 KB TEST.BIN) | Full EE + IOP module tests (IOP reads broken — wrong mmode arg) |
| Run 4 | 2026-03-16 | PCSX2 v2.5.336 | Custom test ISO (CD, 512 KB TEST.BIN) | Full EE + IOP module tests (IOP mmode fixed) |
| Run 5 | 2026-03-18 | PCSX2 v2.5.336 | Custom test ISO (CD, 512 KB TEST.BIN) | IOP only — confirmed fio_read=2048 for all versions after fixing reentrant read sync |
| Run 6 | 2026-03-18 | PCSX2 v2.5.336 | Custom test ISO (CD, 512 KB TEST.BIN) | IOP only — streaming callback test (sceCdSt* API) added |
| Run 7 | 2026-03-19 | PCSX2 v2.5.336 | Custom test ISO (CD, 512 KB TEST.BIN) | IOP only — streaming test with full reason array (bankmax=4) |
| Run 8 | 2026-03-19 | PCSX2 v2.5.336 | Custom test ISO (CD, 512 KB TEST.BIN) | IOP only — parameterized streaming sweep: bufmax {16,32,64,128} × bankmax {2,3,4} × sectors {1..64} on 6 representative firmwares |

BIOS: Europe v02.00 (14/06/2004)

---

## Module Layout Table

| Version | MAN ver | man text | man BSS | FSV ver | fsv text | Free IOP | mmode | search |
|---|---|---:|---:|---|---:|---:|:---:|:---:|
| bios_baseline | 0x0104 | 19,760 | 91,184 | 0x0104 | 18,816 | 1772 KiB | 0 | 1† |
| ioprp14 | 0x0106 | 14,528 | 91,184 | 0x0105 | 9,008 | 1790 KiB | 0 | 1 |
| ioprp15 | 0x0207 | 19,616 | 91,484 | 0x0207 | 15,168 | 1774 KiB | 0 | 1 |
| ioprp16 | 0x020a | 21,536 | 91,500 | 0x020a | 18,656 | 1767 KiB | 0 | 1 |
| **← mmode added →** | | | | | | | | |
| ioprp165 | 0x020b | 24,752 | 91,532 | 0x020b | 19,072 | 1763 KiB | 1 | 1 |
| ioprp202 | 0x020d | 25,360 | 91,568 | 0x020d | 19,424 | 1762 KiB | 1 | 1 |
| ioprp205 | 0x020e | 25,888 | 91,568 | 0x020e | 19,664 | 1756 KiB | 1 | 1 |
| ioprp21  | 0x020e | 25,952 | 91,568 | 0x020e | 19,664 | 1754 KiB | 1 | 1 |
| ioprp210 | 0x020e | 25,952 | 91,568 | 0x020e | 19,664 | 1754 KiB | 1 | 1 |
| ioprp211 | 0x0210 | 27,712 | 92,360 | 0x0210 | 19,584 | 1752 KiB | 1 | 1 |
| ioprp213 | 0x0210 | 27,712 | 92,360 | 0x0210 | 19,584 | 1752 KiB | 1 | 1 |
| ioprp214 | 0x0210 | 27,712 | 92,360 | 0x0210 | 19,584 | 1749 KiB | 1 | 1 |
| **← search broken →** | | | | | | | | |
| ioprp224 | 0x0214 | 30,368 | 91,660 | 0x0214 | 19,744 | 1742 KiB | 1 | **0** |
| ioprp23  | 0x0215 | 30,720 | 91,660 | 0x0215 | 19,440 | 1737 KiB | 1 | **0** |
| **← BSS shrink →** | | | | | | | | |
| ioprp234 | 0x0216 | 33,552 | **51,900** | 0x0216 | 21,568 | 1727 KiB | 1 | 1 |
| ioprp241 | 0x0218 | 35,904 | 51,920 | 0x0218 | 21,696 | 1725 KiB | 1 | 1 |
| ioprp242 | 0x0219 | 35,904 | 51,920 | 0x0219 | 21,744 | 1724 KiB | 1 | 1 |
| ioprp243 | 0x021a | 36,096 | 51,920 | 0x021a | 21,744 | 1723 KiB | 1 | 1 |
| ioprp250 | 0x021c | 39,520 | 52,016 | 0x021c | 22,576 | 1715 KiB | 1 | 1 |
| ioprp253 | 0x021d | 40,944 | 52,016 | 0x021d | 22,624 | 1713 KiB | 1 | 1 |
| ioprp255 | 0x021d | 40,944 | 52,016 | 0x021d | 22,624 | 1715 KiB | 1 | 1 |
| **← BSS shrink again →** | | | | | | | | |
| ioprp260 | 0x0220 | 40,224 | **35,168** | 0x0220 | 17,072 | 1778 KiB | 1 | 1 |
| ioprp271   | 0x0222 | 47,328 | 35,584 | 0x0222 | 17,056 | 1769 KiB | 1 | 1 |
| ioprp271_2 | 0x0222 | 47,248 | **54,304** | 0x0222 | 17,072 | 1750 KiB | 1 | 1 |
| ioprp280   | 0x0223 | 48,112 | 54,304 | 0x0223 | 19,024 | 1748 KiB | 1 | 1 |
| ioprp300   | 0x0225 | 49,504 | 54,304 | 0x0225 | 19,408 | 1744 KiB | 1 | 1 |
| ioprp300_2 | 0x0226 | 50,464 | 54,320 | 0x0226 | 19,872 | 1743 KiB | 1 | 1 |
| ioprp300_3 | 0x0226 | 50,528 | 54,320 | 0x0226 | 19,872 | 1742 KiB | 1 | 1 |
| ioprp300_4 | 0x0225 | 49,888 | 54,320 | 0x0225 | 19,408 | 1744 KiB | 1 | 1 |
| ioprp310   | 0x0226 | 50,464 | 54,320 | 0x0226 | 19,872 | 1742 KiB | 1 | 1 |
| dnas280    | 0x0223 | 49,728 | 54,304 | 0x0223 | 19,376 | 1746 KiB | 1 | 1 |
| dnas300    | 0x0225 | 51,440 | 54,320 | 0x0225 | 19,776 | 1742 KiB | 1 | 1 |
| dnas300_2  | 0x0226 | 52,896 | 54,320 | 0x0226 | 20,320 | 1740 KiB | 1 | 1 |

† bios_baseline search_ret is garbage (141968) — `sceCdSearchFile` called without `sceCdMmode`; mmode_ret=0 means old cdvdman ignores the call.

**Columns:** `mmode` = `sceCdMmode(SCECdMmodeCd)` return value. `search` = `sceCdSearchFile` success (1=found, 0=not found).

---

## EE Behavior Table (Run 2, disc present)

All 31 successfully tested versions are **identical** on every measured behavioral metric:

| Metric | Value | Notes |
|--------|-------|-------|
| `init_ret` | 1 | All versions |
| `disk_type` | 18 | All versions |
| `sceCdRead()` return | 1 | Read queued successfully |
| `cb_before_sync_100ms_poll` | 0 | Callback does NOT fire before sync |
| `cb_after_sync` | 1 | Callback fires AFTER sync completes |
| `cb_reason` | 1 | `SCECdFuncRead` (=1) |
| `sync_ret` | 0 | `sceCdSync(0)` completes without timeout |
| `sync0_ticks` | ~61,000,000 | 128 sectors @ 147 MHz ≈ 415 ms |
| `sync1_poll_count` | ~1,850,000–1,870,000 | `sceCdSync(1)` polling iterations per read |
| `nosync_byte_immediate` | 0xaa | Buffer not yet written immediately after `sceCdRead()` |
| `nosync_byte_after_spin` | 0x31 | Buffer **IS** written before sync returns (DMA completes early) |
| `nosync_byte_after_sync` | 0x31 | Buffer confirmed valid after sync |
| `read2_while_busy_ret` | 0 | Concurrent read rejected |
| `err_after_sync` | 0 | No error |

Versions skipped (search_ret=0, read tests not run): **ioprp224**, **ioprp23**

---

## IOP Test Table (Run 4 — mmode fixed; Run 5 — fio confirmed)

IOP test module (CDVD.IRX) loaded after EE tests for each version.
All versions have `read_ret=1` (IOP reads work across the board).
`fio_read` column updated in Run 5: all versions return 2048 when the reentrant read is properly
awaited before the ioman open (see Finding 8).

| Version | mmode_ret | cb_intr_ctx | cb_thread_id | cb_sync_inside | cb_reenter | sync_ret | open_ret | fio_read |
|---|:---:|:---:|---:|:---:|:---:|:---:|:---:|---:|
| bios_baseline | 18 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp14 | 18 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp15 | 18 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp16 | 18 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp165 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp202 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp205 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp21  | 1 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp210 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp211 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp213 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp214 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp224 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp23  | 1 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp234 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp241 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp242 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp243 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp250 | 1 | **1** | -100 | 0 | **1** | -1 † | 2 | **2048** |
| ioprp253 | 1 | **1** | -100 | 0 | **1** | -1 † | 2 | **2048** |
| ioprp255 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp260 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp271   | 1 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp271_2 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp280   | 1 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp300   | 1 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp300_2 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp300_3 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| ioprp300_4 | 1 | **1** | -100 | 0 | **1** | -1 † | 2 | **2048** |
| ioprp310   | 1 | **1** | -100 | 0 | **1** | 0 | 2 | **2048** |
| dnas280    | 1 | **1** | -100 | 0 | **1** | -1 † | 2 | **2048** |
| dnas300    | 1 | **1** | -100 | 0 | **1** | -1 † | 2 | **2048** |
| dnas300_2  | 1 | **1** | -100 | 0 | **1** | -1 † | 2 | **2048** |

† `sync_ret=-1` = SYNC_TIMEOUT on IOP side (5-second timeout hit waiting for reentrant read).
  `sceCdStop()` is called on timeout, which clears cdvdman busy state, allowing subsequent ioman open to succeed.

**Column notes:**
- `mmode_ret`: `sceCdMmode(SCECdMmodeCd)` return — 18=old cdvdman (no-op/wrong), 1=accepted
- `cb_intr_ctx`: `QueryIntrContext()` from inside callback — **1=interrupt context for ALL versions**
- `cb_thread_id`: -100 for all (confirms interrupt context — no active thread)
- `cb_sync_inside`: `sceCdSync(1)` from inside callback — 0=done (drive idle by callback time)
- `cb_reenter`: `sceCdRead()` return from inside callback — **1=accepted for ALL versions**
- `sync_ret`: `iop_cdvd_sync()` result after waiting for reentrant read — 0=ok, -1=timeout
- `open_ret`: IOP `open("cdrom0:\\TEST.BIN;1")` — 2=success, -16=EACCES/busy, -1=error
- `fio_read`: bytes read — 2048=success, 0=EOF/no data, -1=read error, —=open failed

---

## IOP Streaming Table (Run 7 — sceCdSt* full reason array)

`sceCdStInit(32,4,buf)` + `sceCdStStart(lsn,&mode)` + 200ms wait + 4× `sceCdStRead(4,...)`
+ `sceCdStStop()`. Callback registered across the entire sequence with all reasons recorded.

| Version | st_init | st_start | cb_count | reasons (in order) |
|---|:---:|:---:|:---:|:---|
| bios_baseline | 1 | **0** | 7 | `1 1 1 1 1 1 1` |
| ioprp14 | 1 | 1 | 7 | `1 1 1 1 1 1 1` |
| ioprp15 | 1 | 1 | 8 | `1 1 1 1 1 1 1 1` |
| ioprp16 | 1 | 1 | 8 | `1 1 1 1 1 1 1 1` |
| ioprp165 | 1 | 1 | 8 | `1 1 1 1 1 1 1 1` |
| **← sceCdStStop fires reason=8 →** | | | | |
| ioprp202 | 1 | 1 | 9 | `1 1 1 1 1 1 1 1 8` |
| ioprp205 | 1 | 1 | 9 | `1 1 1 1 1 1 1 1 8` |
| ioprp21  | 1 | 1 | 9 | `1 1 1 1 1 1 1 1 8` |
| ioprp210 | 1 | 1 | 9 | `1 1 1 1 1 1 1 1 8` |
| ioprp211 | 1 | 1 | 9 | `1 1 1 1 1 1 1 1 8` |
| ioprp213 | 1 | 1 | 9 | `1 1 1 1 1 1 1 1 8` |
| ioprp214 | 1 | 1 | 9 | `1 1 1 1 1 1 1 1 8` |
| ioprp224 | 1 | 1 | 8 | `1 1 1 1 1 1 1 8` |
| ioprp23  | 1 | 1 | 8 | `1 1 1 1 1 1 1 8` |
| **← cb_count drops to STREAM_READS →** | | | | |
| ioprp234 | 1 | 1 | **4** | `1 1 1 1` |
| ioprp241 | 1 | 1 | **4** | `1 1 1 1` |
| ioprp242 | 1 | 1 | **4** | `1 1 1 1` |
| ioprp243 | 1 | 1 | **4** | `1 1 1 1` |
| ioprp250 | 1 | 1 | **4** | `1 1 1 1` |
| ioprp253 | 1 | 1 | **4** | `1 1 1 1` |
| ioprp255 | 1 | 1 | **4** | `1 1 1 1` |
| **← sceCdSt* moved to cdvdstm.irx (lib 2.7+) — cdvdman stub returns 0 →** | | | | |
| ioprp260   | **0** | 0 | 0 | — |
| ioprp271   | **0** | 0 | 0 | — |
| ioprp271_2 | **0** | 0 | 0 | — |
| ioprp280   | **0** | 0 | 0 | — |
| ioprp300   | **0** | 0 | 0 | — |
| ioprp300_2 | **0** | 0 | 0 | — |
| ioprp300_3 | **0** | 0 | 0 | — |
| ioprp300_4 | **0** | 0 | 0 | — |
| ioprp310   | **0** | 0 | 0 | — |
| dnas280    | **0** | 0 | 0 | — |
| dnas300    | **0** | 0 | 0 | — |
| dnas300_2  | **0** | 0 | 0 | — |

**Column notes:**
- `st_init`: `sceCdStInit(32, 4, buf)` return — 1=ok, 0=cdvdman stub (functions moved to cdvdstm.irx in lib 2.7+)
- `st_start`: `sceCdStStart(lsn, &mode)` return — 1=ok, 0=failed (bios_baseline: mmode not set)
- `cb_count`: total callback invocations during the streaming session
- `reasons`: ordered list — 1=`SCECdFuncRead`, 8=`SCECdFuncBreak`

Note: `sceCdStRead(4, buf, 0, &err)` returned non-1 for all versions (stream_ok=0). For Era 1/2
this is likely a non-blocking return with 0 sectors; for Era 3 (ioprp234–255) the cb_count=4
matches STREAM_READS exactly, suggesting each sceCdStRead call blocks, fires 1 callback, then
returns the sector count (4 ≠ 1 → breaks the loop after one call with 4 callbacks total).

---

## IOP Streaming Parameter Sweep (Run 8)

Parameterized sweep across 6 representative firmwares:
`bufmax` ∈ {16, 32, 64, 128} × `bankmax` ∈ {2, 3, 4} × `sectors-per-read` ∈ {1, 2, 4, 8, 16, 32, 64 where ≤ bufmax}.
`STREAM_READS = 4`, pacing delay = `nsec × 1302 µs` (~1.5 MiB/s) between reads, 200 ms pre-fill wait.

### Ring buffer geometry

`sceCdStInit(bufmax, bankmax, buf)` prints the actual allocation:

| bufmax | bankmax | bank size (sectors) | total (sectors) |
|-------:|-------:|--------------------:|----------------:|
| 16 | 2 | 8 | 16 |
| 16 | 3 | 5 | 15 |
| 16 | 4 | 4 | 16 |
| 32 | 2 | 16 | 32 |
| 32 | 3 | 10 | 30 |
| 32 | 4 | 8 | 32 |
| 64 | 2 | 32 | 64 |
| 64 | 3 | 21 | 63 |
| 64 | 4 | 16 | 64 |
| 128 | 2 | 64 | 128 |
| 128 | 3 | 42 | 126 |
| 128 | 4 | 32 | 128 |

**Formula:** `bank_size = floor(bufmax / bankmax)` sectors; `total = bank_size × bankmax`.
When `bankmax` does not divide `bufmax` evenly, the remainder sectors are silently discarded.

### sceCdStRead success (`ok`)

`sceCdStRead(nsec, buf, 0 /*non-blocking*/, &err)` returns 1 only for `nsec=1` in all tested
firmware/buffer combinations. All `nsec ≥ 2` return non-1 (`ok=0`), regardless of bufmax or
bankmax. The mode=0 (non-blocking) call requires all `nsec` sectors to be immediately available
in the ring buffer; because our pacing drains sectors faster than PCSX2's simulated drive refills
them for larger reads, those calls fail.

### Callback count vs. sectors-per-read

The central finding of Run 8: **for ioprp165 and all later firmware, the callback count is
completely independent of the sectors-per-read parameter**. The same `cb_count` is observed
for `S=1` and `S=64` within the same `(bufmax, bankmax)` group.

Summary table — `cb_count` and reasons for `B=32` across all bankmax values (all S give the same
result except where noted):

| Firmware | K=2 (all S) | K=3 (all S) | K=4 (all S) | reasons |
|---|:---:|:---:|:---:|:---|
| bios_baseline | 5–6† | 6 | 6–9† | all `1` |
| ioprp14 | 5–6† | 6 | 6–9† | all `1` |
| ioprp165 | 5 | 6 | 7 | all `1` |
| ioprp214 | 6 | 7 | 8 | `(K+3)×1` then `8` |
| ioprp23  | 6 | 7 | 8 | `(K+3)×1` then `8` |
| ioprp255 | 2 | 3 | 4 | all `1` (reason=8 appears for large S‡) |

† **bios_baseline / ioprp14**: callback count grows with test duration — larger S means longer
pacing delays means more bank fills means more callbacks. They do not have the fixed-count
behaviour of later firmware.

‡ **ioprp255**: reason=8 (`SCECdFuncBreak`) occasionally appears as the final callback for
long-running tests (e.g. K=3, S=32), suggesting `sceCdStStop` fires reason=8 asynchronously
when the stop overlaps with an in-progress bank fill; for short tests it resolves synchronously
with no extra callback.

### Callback count vs. bankmax and bufmax

For **ioprp255** (clearest pattern), `cb_count = bankmax` across all tested bufmax values:

| bufmax | K=2 | K=3 | K=4 |
|-------:|:---:|:---:|:---:|
| 16  | 2 | 3 | 4 |
| 32  | 2 | 3 | 4 |
| 64  | 2 | 3 | 4 |
| 128 | 2 | 3 | 4 |

**Interpretation:** exactly one `SCECdFuncRead` callback fires per bank once it is initially
filled by the drive. After the initial fill of all banks, no further callbacks are generated
as sectors are consumed and re-filled.

For **ioprp165 / ioprp214 / ioprp23**, the count is `bankmax + X` where X decreases as
bufmax increases (larger banks take longer to fill, fewer complete in the test window):

| bufmax | ioprp165 K=2 | ioprp214 K=2 (reason=1 only) |
|-------:|:---:|:---:|
| 16  | 5 (or 6 for S=1) | 6 (or 7 for S=1) |
| 32  | 5 | 5 |
| 64  | 4 | 4 |
| 128 | 2 | 2 |

The extra callbacks above `bankmax` reflect additional bank-complete events driven by the
drive continuing to fill banks during the 200 ms pre-fill window.

---

## Finding 1: IOP Callback Context (CONFIRMED for ALL versions — Run 4)

**`cb_intr_ctx=1` (interrupt) and `cb_reenter=1` (reentry works) for every single IOPRP version.**

There is **no split** — the IOP callback always fires from interrupt context across all 33 versions:

```
cb_intr_ctx:   1     ← called from DMAC interrupt handler (ALL versions)
cb_thread_id:  -100  ← GetThreadId() from interrupt = -100 (ALL versions)
cb_sync_inside: 0    ← sceCdSync(1) returns "done" inside callback (ALL versions)
cb_reenter:    1     ← sceCdRead() SUCCEEDS from inside interrupt callback (ALL versions)
```

The Run 3 data showing cb_intr_ctx=0 for ioprp165+ was a false reading — reads were broken
(wrong `sceCdMmode` argument), so the callback never fired and the variable kept its
zero-initialised default.

### Neutrino implication
The IOP cdvdman callback is **always called from interrupt context** across all PS2 BIOS/IOPRP
versions (v0.1.04 through v0.2.26). Games that call `QueryIntrContext()` inside their callback
to decide between `iSignalSema` and `SignalSema` will always take the interrupt path.
`cdvdman_emu` dispatching the callback from a thread context is incorrect for all versions.

---

## Finding 2: EE Callback Timing (NEW — Run 2)

The EE-side callback fires **after** `sceCdSync(0)` returns, not before:

```
cb_before_sync_100ms_poll = 0   ← not fired during 100ms poll before sync
cb_after_sync              = 1   ← fired after sync completes
cb_reason                  = 1   ← SCECdFuncRead
```

This is **uniform across all 31 tested IOPRP versions** — no behavioral difference found.

### Neutrino implication
The EE callback thread must be able to run while the main thread blocks in `sceCdSync`. In PCSX2
testing, a thread priority issue causes deadlock: the callback thread never gets CPU time because
the main thread spins in `sceCdSync(1)`. A yield (nanosleep/nap) in the sync polling loop is
required. On real hardware this is not an issue (preemptive scheduling), but neutrino's EE
callback dispatch must not be blocked by the calling thread's priority.

---

## Finding 3: DMA Completes Before Sync Signals

```
nosync_byte_immediate = 0xaa   ← buffer unwritten immediately after sceCdRead()
nosync_byte_after_spin = 0x31  ← buffer written before sceCdSync(0) returns
nosync_byte_after_sync = 0x31  ← same value confirmed after sync
```

The DMA transfer completes and writes data to the buffer **before** `sceCdSync` signals done.
The sync primitive waits for drive/controller readiness, not for the DMA transfer itself.
This means a brief spin after `sceCdRead` may yield valid data without `sceCdSync` — but this
is not guaranteed behavior.

---

## Finding 4: sceCdRead() Behavior With No Disc (Run 1)

Three distinct regimes:

| Regime | Versions | Return | Error | Notes |
|---|---|---|---|---|
| **Old** | bios–ioprp16 (≤v0x020a) | **1** (queued) | 18 via callback | Starts anyway, error via callback later |
| **Middle** | ioprp165–ioprp214 (v0x020b–v0x0210) | 0 (fail) | **0** (silent) | Immediate fail, no error code set |
| **New** | ioprp224+ (≥v0x0214) | 0 (fail) | **254** (0xFE) | Immediate fail with explicit error |

Transition from silent fail (err=0) to explicit error (err=254): between ioprp214 (v0x0210)
and ioprp224 (v0x0214).

### Neutrino implication
`cdvdman_emu` returns 0 on failure — correct for modern firmware. Games targeting old firmware
expect `sceCdRead` to return 1 and receive an error via callback.

---

## Finding 5: sceCdMmode Support Added at ioprp165

```
ioprp14/15/16  (v≤0x020a): mmode_ret = 0  ← call not supported / no-op
ioprp165+      (v≥0x020b): mmode_ret = 1  ← call accepted
```

`sceCdSearchFile` requires `sceCdMmode` to be called first on ioprp165+ to properly enumerate
the disc. Without it, `search_ret=0` even though disk detection succeeds.

**Exception — ioprp224, ioprp23 (v0x0214–v0x0215):** `sceCdMmode` succeeds (ret=1) but
`sceCdSearchFile` still returns 0. PCSX2-specific emulation gap for these two versions.

---

## Finding 6: Concurrent Reads (Run 1, no disc)

| Regime | Versions | 2nd read while 1st in progress |
|---|---|---|
| **Old** | bios–ioprp16 (≤v0x020a) | Returns **1** — second read queued/accepted |
| **New** | ioprp165+ (≥v0x020b) | Returns 0 — second read rejected immediately |

With a disc (Run 2): all versions return `read2_while_busy_ret=0` — no change, concurrent
reads are always rejected when cdvdman has a pending operation.

---

## Finding 7: Memory Layout Milestones

### cdvdman (cdvd_driver) text size growth
```
ioprp14  v0.1.6  14,528b  ← smallest — old era
ioprp15  v0.2.7  19,616b  ← +5 KB: v2.x rewrite
ioprp16  v0.2.10 21,536b
ioprp165 v0.2.11 24,752b  ← sceCdMmode added
ioprp224 v0.2.14 30,368b
ioprp234 v0.2.16 33,552b  ← BSS restructured (91 KB → 52 KB)
ioprp250 v0.2.1c 39,520b
ioprp260 v0.2.20 40,224b  ← second BSS drop (52 KB → 35 KB)
ioprp310 v0.2.26 50,464b  ← largest
```
Text grew **3.5× from ioprp14 to ioprp310**.

### cdvdman BSS restructuring
```
~91,500b  bios–ioprp23     (monolithic static buffers)
~51,900b  ioprp234–255     (-40 KB at v0.2.16 — large buffer moved or dynamic)
~35,200b  ioprp260–271     (-17 KB at v0.2.20 — further reduction)
~54,300b  ioprp271_2–310   (+19 KB at v0.2.22 alt build — different target hardware)
```

**ioprp271 vs ioprp271_2** — same version (v0x0222), BSS 35,584b vs 54,304b. Two builds
for different hardware revisions (e.g., DTL vs consumer, or fat vs slim PS2).

### IOP free memory after CDVD modules loaded
```
ioprp14       1790 KiB  ← most free (small cdvdman v0.1.6)
ioprp260      1778 KiB  ← local maximum after BSS reduction
bios_baseline 1772 KiB
ioprp253      1713 KiB  ← least free (peak cdvdman v0.2.1d)
```
All versions leave >1700 KiB free for game modules.

### cdvdman load address evolution
```
bios/ioprp14  0x1ce30   (IOP OS v0.9.1, small kernel)
ioprp15       0x1d830
ioprp205      0x1e330
ioprp224      0x1f430
ioprp260      0x22330   (IOP OS v2.2, larger kernel)
ioprp310      0x22930
```

---

## Finding 8: IOP ioman fio_read = 2048 for ALL Versions (Run 5)

`open("cdrom0:\\TEST.BIN;1") + read(2048)` returns exactly 2048 bytes across **all 33 IOPRP
versions** when the reentrant `sceCdRead` (issued from inside the callback) is fully synced
before the ioman open is attempted.

```
fio_read = 2048   ALL versions (bios_baseline through ioprp310, dnas variants)
```

Run 4 variability (0 / -1 / -16 for many versions) was caused by the reentrant `sceCdRead`
leaving cdvdman in a busy state. The ioman cdrom0 driver internally calls cdvdman, so an
outstanding read caused open or read to fail. Once `iop_cdvd_sync()` is called for the
reentrant read before proceeding to the ioman section, all versions cleanly return 2048.

For SYNC_TIMEOUT versions (ioprp250/253/dnas280/dnas300/300_4/dnas300_2): the timeout handler
calls `sceCdStop()` which clears cdvdman busy state, allowing the ioman open to succeed
despite the reentrant read not having completed normally.

### Neutrino implication
`cdvdman_emu`'s ioman/cdrom0 interface must not be called while a cdvdman read is in progress.
The open/read path must internally wait for any outstanding operation to complete.

---

## Finding 9: sceCdSt* Streaming — Four Behavioral Eras (Runs 7 + 8)

The streaming API (`sceCdSt*`) divides across firmware versions into four distinct regimes.
Full reason arrays (Run 7) and the parameter sweep (Run 8) together establish:

1. **All streaming callbacks are reason=1 (`SCECdFuncRead`)** — in every era.
2. **Callback count is driven by `bankmax`, not by `sectors-per-read`** — for ioprp165+.
3. **`sceCdStStop()` may add one final reason=8** — era-dependent (see below).

### Era 0: bios_baseline, ioprp14 (v≤0x0106)
```
st_init=1, st_start=0 (bios) / 1 (ioprp14)
cb_count grows with test duration — not a fixed count per session
```
Callback fires once per bank fill event. The count is **not fixed**: it increases with
test duration because the drive keeps filling banks and each fill triggers a new callback.
Longer sector reads mean longer pacing delays mean more bank fills mean more callbacks.
`sceCdStStop()` fires reason=1 (not reason=8) — stop uses the same reason code as reads.

Note: `st_start=0` on bios_baseline because `sceCdMmode` was not called before streaming.

### Era 1: ioprp165 (v0x020b)
```
st_init=1, st_start=1
cb_count = bankmax + X  (X depends on bufmax; X≈3 for bufmax=32, X→0 for bufmax=128)
all reason=1, independent of sectors-per-read
```
Callback fires once per bank fill. The count stabilises to a **fixed value per
(bufmax, bankmax)** pair — the drive does not keep triggering callbacks indefinitely.
The "extra" callbacks above `bankmax` reflect bank fills during the 200 ms pre-fill window
before the application begins reading. `sceCdStStop()` fires no reason=8.

### Era 2: ioprp202–23 (v0x020d–0x0215)
```
st_init=1, st_start=1
cb_count = bankmax + X + 1  (same X as Era 1, plus one final reason=8)
reasons: (bankmax+X)×1  then  8
```
Identical behaviour to Era 1 for streaming reads. However, `sceCdStStop()` always fires
`SCECdFuncBreak` (8) as the **final** callback — always last regardless of bufmax/bankmax/
sectors-per-read. This is completely consistent across the entire sweep.

### Era 3: ioprp234–255 (v0x0216–0x021d)
```
st_init=1, st_start=1
cb_count = bankmax  (exactly — one callback per bank, initial fill only)
all reason=1; reason=8 appears only for long-running tests (timing-dependent)
```
The clearest regime. `cb_count = bankmax` exactly across all tested bufmax values (16–128)
and all sector sizes. The drive fires one `SCECdFuncRead` callback per bank on initial fill;
once all banks are filled no further callbacks are generated during streaming.
`sceCdStStop()` fires reason=8 only asynchronously (when a bank fill overlaps the stop).
For short-running tests the stop resolves synchronously with no callback.

Run 7 observed `cb_count=4` with `bankmax=4` — now confirmed as `cb_count = bankmax`.
The earlier interpretation ("one callback per sceCdStRead call") was incorrect.

### Era 4: ioprp260+ (v≥0x0220) and all dnas variants
```
st_init=0, st_start=0, cb_count=0
```
`sceCdStInit` returns 0 because the streaming functions moved out of cdvdman. Per the
official PS2 SDK documentation: *"Starting with Library Release 2.7, processing for the
streaming function group was split off into the cdvdstm.irx module."*

The test module imports `sceCdSt*` from cdvdman; on ioprp260+ those slots are stubs that
return 0. The streaming API is intact — it simply lives in `cdvdstm.irx` instead.
The transition point (cdvdman v0x021d → v0x0220) matches exactly where st_init goes to 0.

### Mechanism: streaming uses sceCdRead internally (confirmed by CDVD read log, Run 8)

PCSX2 CDVD read logging directly shows which `CDRead` calls are issued during the streaming
test. The block sizes in the log confirm the mechanism:

| Firmware | CDRead block size | Bank-aware? |
|---|---|---|
| bios_baseline, ioprp14 | **always 16 sectors** (hardcoded) | No |
| ioprp165, ioprp214, ioprp23 | **= `bank_size`** for bank≥16sec; 16 for smaller banks | Yes (with floor) |
| ioprp255 | **= `bank_size` exactly** for every tested configuration | Yes (exact) |

For ioprp255 (Era 3), every single CDRead issues exactly `floor(bufmax/bankmax)` sectors —
the bank size — across all 12 tested (bufmax, bankmax) combinations. `cb_count = bankmax`
because the drive issues exactly one CDRead per bank on start, and does not issue further
reads proactively.

For ioprp165/214/23 (Eras 1–2), the dominant CDRead block size equals `bank_size` for all
configurations with bank_size ≥ 16 sectors. For very small banks (bank < 16) the read size
floors at 16 sectors. Additional smaller or larger reads visible in some windows are
background reads from the previous test iteration bleeding through (these eras pre-fill
aggressively, so reads issued during one (bufmax, bankmax) session overlap the next).

For bios/ioprp14 (Era 0), the CDRead block size is hardcoded at 16 sectors regardless of
bank geometry. The streaming implementation in these early versions is not bank-aware.

All streaming callbacks are reason=1 — the same as a direct `sceCdRead` completion callback.
There is no separate streaming callback reason. Each CDRead completion fires the registered
callback through the same path as an explicit sceCdRead.

This explains every observation:

- **All reasons = 1**: callback is the sceCdRead completion, fired once per CDRead.
- **CDRead block = bank_size**: streaming issues one internal sceCdRead per bank of exactly
  `floor(bufmax/bankmax)` sectors.
- **cb_count = bankmax** (Era 3): one CDRead per bank on start, no further reads.
- **cb_count = bankmax + X** (Era 1/2): X additional CDReads are issued during the pre-fill
  window; X shrinks as bank_size grows (larger banks take longer to fill in the same window).
- **Era 0 continuous growth**: streaming keeps issuing new 16-sector CDReads as each completes,
  reading indefinitely regardless of application consumption.
- **reason=8 from sceCdStStop() (Era 2)**: `StStop` calls `sceCdBreak` to abort the in-flight
  background CDRead, which fires reason=8 through the break path. Era 3 does not do this.

### Neutrino implication
`cdvdman_emu`'s streaming should drive itself by calling its own internal `sceCdRead` path —
one call per bank. This naturally fires the user callback with reason=1 for each bank fill
with no special streaming callback code needed. The target read-ahead is `bankmax` once on
start (Era 3 behaviour): fill all banks initially, then issue one new internal `sceCdRead` per
bank whenever `sceCdStRead` drains it.

`cdvdman_emu`'s `sceCdStStop()` fires `SCECdFuncRead` instead of `SCECdFuncBreak` (8).
For Era 2 firmware (ioprp202–23) games that dispatch on the stop callback reason, neutrino
will deliver the wrong value. Fix: route `sceCdStStop()` through the internal break path.

For Era 4 (lib 2.7+ firmware): games that load `cdvdstm.irx` expect the streaming functions
to be exported by a separate `cdvdstm` library, not `cdvdman`. Neutrino already has a
`cdvdstm` export table stub (`exports.tab:209`) — it needs to be populated with the actual
streaming implementation to support these games.

---

## Still Needed

- **IOP SYNC_TIMEOUT on ioprp250/253/dnas280/dnas300/300_4/dnas300_2**: reentrant read from
  callback does not complete within 5 seconds on these versions. May indicate cdvdfsv interference
  or a version-specific reentrant read restriction. `sceCdStop()` recovers cdvdman state so
  subsequent ioman access still works, but the root cause is unknown.
- **EE `cb_called_by_fio`**: whether `open("cdrom0:\\...")/read()` triggers `sceCdCallback`
  needs a dedicated test run capturing that specific value across all versions.
- **ioprp224/ioprp23 EE search failure**: `sceCdSearchFile` returns 0 — PCSX2-specific gap
  or genuine firmware difference.
- **Real hardware validation**: interrupt-context callback, thread priority behaviour, and
  ioprp path loading confirmed in PCSX2 but not yet on hardware.
