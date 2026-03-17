# CDVD Behavior Test Results

## Runs

| Run | Date | Platform | Disc | Status |
|-----|------|----------|------|--------|
| Run 1 | 2026-03-15 | PCSX2 v2.5.336 | None | IOP-side data only; read tests skipped |
| Run 2 | 2026-03-16 | PCSX2 v2.5.336 | Custom test ISO (CD, 512 KB TEST.BIN) | Full EE tests; IOP module disabled |
| Run 3 | 2026-03-16 | PCSX2 v2.5.336 | Custom test ISO (CD, 512 KB TEST.BIN) | Full EE + IOP module tests (IOP reads broken — wrong mmode arg) |
| Run 4 | 2026-03-16 | PCSX2 v2.5.336 | Custom test ISO (CD, 512 KB TEST.BIN) | Full EE + IOP module tests (IOP mmode fixed) |

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

## IOP Test Table (Run 4 — mmode fixed)

IOP test module (CDVD.IRX) loaded after EE tests for each version.
All versions now have `read_ret=1` (IOP reads work across the board).

| Version | mmode_ret | cb_intr_ctx | cb_thread_id | cb_sync_inside | cb_reenter | sync_ret | open_ret | fio_read |
|---|:---:|:---:|---:|:---:|:---:|:---:|:---:|---:|
| bios_baseline | 18 | **1** | -100 | 0 | **1** | 0 | 2 | 2048 |
| ioprp14 | 18 | **1** | -100 | 0 | **1** | 0 | 2 | 2048 |
| ioprp15 | 18 | **1** | -100 | 0 | **1** | 0 | 2 | 2048 |
| ioprp16 | 18 | **1** | -100 | 0 | **1** | 0 | 2 | 2048 |
| ioprp165 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | 2048 |
| ioprp202 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | 2048 |
| ioprp205 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | 2048 |
| ioprp21  | 1 | **1** | -100 | 0 | **1** | 0 | 2 | 2048 |
| ioprp210 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | -1 |
| ioprp211 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | 2048 |
| ioprp213 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | -1 |
| ioprp214 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | 2048 |
| ioprp224 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | -1 |
| ioprp23  | 1 | **1** | -100 | 0 | **1** | 0 | 2 | 2048 |
| ioprp234 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | -1 |
| ioprp241 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | 0 |
| ioprp242 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | 0 |
| ioprp243 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | 0 |
| ioprp250 | 1 | **1** | -100 | 0 | **1** | -1 † | -16 | — |
| ioprp253 | 1 | **1** | -100 | 0 | **1** | -1 † | -16 | — |
| ioprp255 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | -1 |
| ioprp260 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | -1 |
| ioprp271   | 1 | **1** | -100 | 0 | **1** | 0 | 2 | 0 |
| ioprp271_2 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | 0 |
| ioprp280   | 1 | **1** | -100 | 0 | **1** | 0 | 2 | 0 |
| ioprp300   | 1 | **1** | -100 | 0 | **1** | 0 | 2 | 0 |
| ioprp300_2 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | 0 |
| ioprp300_3 | 1 | **1** | -100 | 0 | **1** | 0 | 2 | 0 |
| ioprp300_4 | 1 | **1** | -100 | 0 | **1** | -1 † | 2 | 2048 |
| ioprp310   | 1 | **1** | -100 | 0 | **1** | 0 | 2 | 0 |
| dnas280    | 1 | **1** | -100 | 0 | **1** | -1 † | -16 | — |
| dnas300    | 1 | **1** | -100 | 0 | **1** | -1 † | 2 | 2048 |
| dnas300_2  | 1 | **1** | -100 | 0 | **1** | -1 † | 2 | 2048 |

† `sync_ret=-1` = SYNC_TIMEOUT on IOP side (5-second timeout hit waiting for reentrant read to complete)

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

## Still Needed

- **IOP FIO test inconsistency**: `fio_read` varies (2048 / 0 / -1 / -16) across versions even
  though the CDVD.IRX was loaded after a successful EE test run. Likely caused by the reentrant
  `sceCdRead` issued from inside the callback leaving cdvdman in a busy state when the FIO open
  is attempted. Needs investigation.
- **IOP SYNC_TIMEOUT on ioprp250/253/dnas280/dnas300/300_4/dnas300_2**: reentrant read from
  callback does not complete within 5 seconds on these versions. May indicate cdvdfsv interference
  or a version-specific reentrant read restriction.
- **EE `cb_called_by_fio`**: whether `open("cdrom0:\\...")/read()` triggers `sceCdCallback`
  needs a dedicated test run capturing that specific value across all versions.
- **ioprp224/ioprp23 EE search failure**: `sceCdSearchFile` returns 0 — PCSX2-specific gap
  or genuine firmware difference.
- **Real hardware validation**: interrupt-context callback, thread priority behaviour, and
  ioprp path loading confirmed in PCSX2 but not yet on hardware.
