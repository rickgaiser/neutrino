/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include "internal.h"

static int AllocBank(void **pointer);
static int ReadSectors(int maxcount, void *buffer);
static int StFillStreamBuffer(void);
static void StStartFillStreamBuffer(void);

static unsigned int StmScheduleCb(void *arg)
{
    return ((StFillStreamBuffer() >= 0) ? 0 : 0x00704000);
}

static void StmCallback(void)
{
    int OldState;

    // Only update parameters if the streaming system was reading. Otherwise, this callback might have been triggered by the game reading data (BUG!)
    if (cdvdman_stat.StreamingData.StIsReading) {
        CpuSuspendIntr(&OldState);
        cdvdman_stat.StreamingData.Stlsn += cdvdman_stat.StreamingData.StBanksize;
        cdvdman_stat.StreamingData.StStreamed += cdvdman_stat.StreamingData.StBanksize;
        cdvdman_stat.StreamingData.StWritePtr += cdvdman_stat.StreamingData.StBanksize;
        if (cdvdman_stat.StreamingData.StWritePtr >= cdvdman_stat.StreamingData.StBufmax)
            cdvdman_stat.StreamingData.StWritePtr = 0;
        cdvdman_stat.StreamingData.StIsReading = 0;
        CpuResumeIntr(OldState);
    }

    //M_DEBUG("StmCallback: %08lx, wr: %u, rd: %u, streamed: %u\n", cdvdman_stat.StreamingData.Stlsn, cdvdman_stat.StreamingData.StWritePtr, cdvdman_stat.StreamingData.StReadPtr, cdvdman_stat.StreamingData.StStreamed);

    StStartFillStreamBuffer();
}

static void StReset(void)
{
    cdvdman_stat.StreamingData.StWritePtr = 0;
    cdvdman_stat.StreamingData.StReadPtr = 0;
    cdvdman_stat.StreamingData.StStreamed = 0;
    cdvdman_stat.StreamingData.StIsReading = 0;
}

// 0 = OK. <0 = error in sceCdRead. >0 = full buffer.
static int StFillStreamBuffer(void)
{
    int result, OldState;
    void *ptr;

    /*	SCEI used a similar design, but their implementation uses a bitmap to mark the filled/empty banks instead
    (which is probably immune to race conditions, but we are interested in saving memory).	*/
    CpuSuspendIntr(&OldState);

    if (cdvdman_stat.StreamingData.StIsReading) {
        CpuResumeIntr(OldState);
        return 0;
    }

    CancelAlarm(&StmScheduleCb, &cdvdman_stat.StreamingData);
    cdvdman_stat.StreamingData.StIsReading = 1;

    // Determine how much more to read.
    result = AllocBank(&ptr);

    CpuResumeIntr(OldState);

    if (result == 0) {
        // M_DEBUG("Stream fill buffer: Stream lsn 0x%08x - %u sectors:%p\n", cdvdman_stat.StreamingData.Stlsn, cdvdman_stat.StreamingData.StBanksize, ptr);
        if (sceCdRead_internal(cdvdman_stat.StreamingData.Stlsn, cdvdman_stat.StreamingData.StBanksize, ptr, NULL, ECS_STREAMING) == 0) {
            // Failed to start reading.
            cdvdman_stat.StreamingData.StIsReading = 0;
            result = -1;
        } else {
            result = 0;
        }
    } else {
        M_DEBUG("Stream fill buffer: Stream full.\n");
        // Nothing else to read.
        cdvdman_stat.StreamingData.StIsReading = 0;
        result = 1;
    }

    return result;
}

static void StStartFillStreamBuffer(void)
{
    iop_sys_clock_t StmScheduleClock;

    if (StFillStreamBuffer() < 0) {
        M_DEBUG("StmCallback: Rescheduling read.\n");
        StmScheduleClock.lo = 0x00704000;
        StmScheduleClock.hi = 0;
        SetAlarm(&StmScheduleClock, &StmScheduleCb, &cdvdman_stat.StreamingData);
    }
}

int sceCdStInit(u32 bufmax, u32 bankmax, void *iop_bufaddr)
{
    int OldState;

    M_DEBUG("%s(%lu, %lu, 0x%X)\n", __FUNCTION__, bufmax, bankmax, iop_bufaddr);

    //M_DEBUG("%s bufmax: %u (%lu), bankmax: %lu, banksize: %u, buffer: %p\n", __FUNCTION__, cdvdman_stat.StreamingData.StBufmax, bufmax, bankmax, cdvdman_stat.StreamingData.StBanksize, iop_bufaddr);

    cdvdman_stat.err = SCECdErNO;

    CancelAlarm(&StmScheduleCb, &cdvdman_stat.StreamingData);

    CpuSuspendIntr(&OldState);
    cdvdman_stat.StreamingData.StBankmax = bankmax;
    cdvdman_stat.StreamingData.StBanksize = bufmax / bankmax;
    cdvdman_stat.StreamingData.StBufmax = cdvdman_stat.StreamingData.StBanksize * cdvdman_stat.StreamingData.StBankmax;
    cdvdman_stat.StreamingData.StIOP_bufaddr = iop_bufaddr;
    StReset();

    CpuResumeIntr(OldState);

    //M_DEBUG("sceCdStInit bufmax: %u (%lu), bankmax: %lu, banksize: %u, buffer: %p\n", cdvdman_stat.StreamingData.StBufmax, bufmax, bankmax, cdvdman_stat.StreamingData.StBanksize, iop_bufaddr);

    return 1;
}

// Must be called from an interrupt-disabled state.
static int AllocBank(void **pointer)
{
    int result;

    //M_DEBUG("%s\n", __FUNCTION__);

    if (cdvdman_stat.StreamingData.StBufmax - cdvdman_stat.StreamingData.StStreamed >= cdvdman_stat.StreamingData.StBanksize) {
        *pointer = cdvdman_stat.StreamingData.StIOP_bufaddr + cdvdman_stat.StreamingData.StWritePtr * 2048;
        result = 0;
    } else {
        *pointer = NULL;
        result = -ENOMEM;
    }

    //M_DEBUG("AllocBank: wrptr: %u, rdptr: %u, streamed: %u\n", cdvdman_stat.StreamingData.StWritePtr, cdvdman_stat.StreamingData.StReadPtr, cdvdman_stat.StreamingData.StStreamed);

    return result;
}

static int ReadSectorsEE(int maxcount, void *buffer)
{
    int OldState, result, dmat_id, dmat_count;
    unsigned short int SectorsToCopy, rdptr;
    SifDmaTransfer_t dmat[2];
    void *ptr;

    //	M_DEBUG("ReadSectors EE: wr: %u, rd: %u, streamed: %u\n", cdvdman_stat.StreamingData.StWritePtr, cdvdman_stat.StreamingData.StReadPtr, cdvdman_stat.StreamingData.StStreamed);

    result = 0;
    ptr = buffer;
    dmat_count = 0;
    dmat_id = 0;

    CpuSuspendIntr(&OldState);
    rdptr = cdvdman_stat.StreamingData.StReadPtr;

    // When Wr <= Rd, the buffer is either full or empty. Check StStreamed.
    if (cdvdman_stat.StreamingData.StWritePtr <= rdptr && cdvdman_stat.StreamingData.StStreamed > 0) {
        SectorsToCopy = cdvdman_stat.StreamingData.StBufmax - rdptr;
        if (SectorsToCopy > maxcount)
            SectorsToCopy = maxcount;
        if (SectorsToCopy > 0) {
            dmat[0].src = cdvdman_stat.StreamingData.StIOP_bufaddr + rdptr * 2048;
            dmat[0].dest = buffer;
            dmat[0].size = SectorsToCopy * 2048;
            dmat[0].attr = 0;
            dmat_count = 1;
            ptr += SectorsToCopy * 2048;
            rdptr += SectorsToCopy;
            if (rdptr >= cdvdman_stat.StreamingData.StBufmax)
                rdptr = 0;
            result += SectorsToCopy;
        }
    }
    // When Rd < Wr, there are sectors in the buffer.
    if (result < maxcount && rdptr < cdvdman_stat.StreamingData.StWritePtr) {
        SectorsToCopy = cdvdman_stat.StreamingData.StWritePtr - rdptr;
        if (SectorsToCopy > maxcount - result)
            SectorsToCopy = maxcount - result;
        if (SectorsToCopy > 0) {
            dmat[dmat_count].src = cdvdman_stat.StreamingData.StIOP_bufaddr + rdptr * 2048;
            dmat[dmat_count].dest = ptr;
            dmat[dmat_count].size = SectorsToCopy * 2048;
            dmat[dmat_count].attr = 0;
            dmat_count++;
            rdptr += SectorsToCopy;
            if (rdptr >= cdvdman_stat.StreamingData.StBufmax)
                rdptr = 0;
            result += SectorsToCopy;
        }
    }

    if (dmat_count > 0)
        while ((dmat_id = sceSifSetDma(dmat, dmat_count)) == 0) {
        };

    CpuResumeIntr(OldState);

    if (dmat_count > 0) // Only if there is data to copy
    {
        while (sceSifDmaStat(dmat_id) >= 0) {
        };

        // Finally, update variables.
        CpuSuspendIntr(&OldState);
        cdvdman_stat.StreamingData.StReadPtr = rdptr;
        cdvdman_stat.StreamingData.StStreamed -= result;
        CpuResumeIntr(OldState);
    }

    return result;
}

static int ReadSectors(int maxcount, void *buffer)
{
    int OldState, result;
    unsigned short int SectorsToCopy;
    void *ptr;

    //	M_DEBUG("ReadSectors: wr: %u, rd: %u, streamed: %u\n", cdvdman_stat.StreamingData.StWritePtr, cdvdman_stat.StreamingData.StReadPtr, cdvdman_stat.StreamingData.StStreamed);

    result = 0;
    ptr = buffer;
    CpuSuspendIntr(&OldState);

    // When Wr <= Rd, the buffer is either full or empty. Check StStreamed.
    if (cdvdman_stat.StreamingData.StWritePtr <= cdvdman_stat.StreamingData.StReadPtr && cdvdman_stat.StreamingData.StStreamed > 0) {
        SectorsToCopy = cdvdman_stat.StreamingData.StBufmax - cdvdman_stat.StreamingData.StReadPtr;
        if (SectorsToCopy > maxcount)
            SectorsToCopy = maxcount;
        if (SectorsToCopy > 0) {
            memcpy(buffer, cdvdman_stat.StreamingData.StIOP_bufaddr + cdvdman_stat.StreamingData.StReadPtr * 2048, SectorsToCopy * 2048);
            ptr += SectorsToCopy * 2048;
            cdvdman_stat.StreamingData.StReadPtr += SectorsToCopy;
            if (cdvdman_stat.StreamingData.StReadPtr >= cdvdman_stat.StreamingData.StBufmax)
                cdvdman_stat.StreamingData.StReadPtr = 0;
            cdvdman_stat.StreamingData.StStreamed -= SectorsToCopy;
            result += SectorsToCopy;
        }
    }
    // When Rd < Wr, there are sectors in the buffer.
    if (result < maxcount && cdvdman_stat.StreamingData.StReadPtr < cdvdman_stat.StreamingData.StWritePtr) {
        SectorsToCopy = cdvdman_stat.StreamingData.StWritePtr - cdvdman_stat.StreamingData.StReadPtr;
        if (SectorsToCopy > maxcount - result)
            SectorsToCopy = maxcount - result;
        if (SectorsToCopy > 0) {
            memcpy(ptr, cdvdman_stat.StreamingData.StIOP_bufaddr + cdvdman_stat.StreamingData.StReadPtr * 2048, SectorsToCopy * 2048);
            cdvdman_stat.StreamingData.StReadPtr += SectorsToCopy;
            if (cdvdman_stat.StreamingData.StReadPtr >= cdvdman_stat.StreamingData.StBufmax)
                cdvdman_stat.StreamingData.StReadPtr = 0;
            cdvdman_stat.StreamingData.StStreamed -= SectorsToCopy;
            result += SectorsToCopy;
        }
    }

    CpuResumeIntr(OldState);

    return result;
}

int sceCdStStart(u32 lsn, sceCdRMode *mode)
{
    int OldState;

    M_DEBUG("%s(%lu, 0x%X)\n", __FUNCTION__, lsn, mode);

    sceCdStStop();

    CpuSuspendIntr(&OldState);

    cdvdman_stat.StreamingData.Stlsn = lsn;
    cdvdman_stat.StreamingData.StStat = 1;
    StReset();
    SetStm0Callback(&StmCallback);
    CpuResumeIntr(OldState);

    cdvdman_stat.err = SCECdErNO;
    cdvdman_stat.status = SCECdStatPause;
    StStartFillStreamBuffer();

    return 1;
}

int sceCdStStat(void)
{
    M_DEBUG("%s() StStreamed = 0x%x\n", __FUNCTION__, cdvdman_stat.StreamingData.StStreamed);

    cdvdman_stat.err = SCECdErNO;
    return cdvdman_stat.StreamingData.StStreamed;
}

int sceCdStStop(void)
{
    int OldState;

    M_DEBUG("%s() StStat = 0x%x\n", __FUNCTION__, cdvdman_stat.StreamingData.StStat);

    cdvdman_stat.err = SCECdErNO;
    cdvdman_stat.status = SCECdStatPause;
    if (cdvdman_stat.StreamingData.StStat) {
        CancelAlarm(&StmScheduleCb, &cdvdman_stat.StreamingData);

        CpuSuspendIntr(&OldState);

        // Stop.
        SetStm0Callback(NULL);
        cdvdman_stat.StreamingData.StStreamed = 0;
        cdvdman_stat.StreamingData.StStat = 0;
        StReset();

        CpuResumeIntr(OldState);

        sceCdSync(0);
    }

    return 1;
}

int sceCdStPause(void)
{
    int OldState;

    M_DEBUG("%s() StStat = 0x%x\n", __FUNCTION__, cdvdman_stat.StreamingData.StStat);

    cdvdman_stat.err = SCECdErNO;
    cdvdman_stat.status = SCECdStatPause;
    if (cdvdman_stat.StreamingData.StStat) {
        CancelAlarm(&StmScheduleCb, &cdvdman_stat.StreamingData);

        CpuSuspendIntr(&OldState);
        // Pause.
        SetStm0Callback(NULL);
        cdvdman_stat.StreamingData.StIsReading = 0;
        CpuResumeIntr(OldState);

        sceCdSync(0);

        return 1;
    } else {
        return 0;
    }
}

int sceCdStResume(void)
{
    int OldState;

    M_DEBUG("%s() StStat = 0x%x\n", __FUNCTION__, cdvdman_stat.StreamingData.StStat);

    cdvdman_stat.err = SCECdErNO;
    cdvdman_stat.status = SCECdStatPause;
    if (cdvdman_stat.StreamingData.StStat) {
        CpuSuspendIntr(&OldState);
        // Resume
        SetStm0Callback(&StmCallback);
        CpuResumeIntr(OldState);

        StStartFillStreamBuffer();

        return 1;
    } else {
        return 0;
    }
}

int sceCdStSeek(u32 lsn)
{
    M_DEBUG("%s(%lu)\n", __FUNCTION__, lsn);

    cdvdman_stat.err = SCECdErNO;
    cdvdman_stat.status = SCECdStatPause;
    if (cdvdman_stat.StreamingData.StStat) {
        return sceCdStStart(lsn, NULL);
    } else {
        return 0;
    }
}

int sceCdStRead(u32 sectors, u32 *buffer, u32 mode, u32 *error)
{
    int SectorsRead, SectorsToRead, result;
    void *ptr;

    M_DEBUG("%s(%lu, 0x%X, %lu, 0x%x)\n", __FUNCTION__, sectors, buffer, mode, error);
    //M_DEBUG("StRead called: sectors %lu:%p, mode: %lu, stat: %u,%u\n", sectors, buffer, mode, cdvdman_stat.StreamingData.StStat, cdvdman_stat.StreamingData.StIsReading);

    cdvdman_stat.err = SCECdErNO;
    if (cdvdman_stat.StreamingData.StStat) {
        SetEventFlag(cdvdman_stat.intr_ef, CDVDEF_STM_DONE);
        for (SectorsToRead = sectors, result = 0, SectorsRead = 0, ptr = (void *)((u32)buffer & ~0x80000000); result < sectors; SectorsToRead -= SectorsRead, ptr += SectorsRead * 2048) {
            WaitEventFlag(cdvdman_stat.intr_ef, CDVDEF_STM_DONE, WEF_AND, NULL);
            ClearEventFlag(cdvdman_stat.intr_ef, ~CDVDEF_STM_DONE);

            //		M_DEBUG("Sectors: %u:%p, mode: %lu", SectorsToRead, ptr, mode);
            if ((u32)buffer & 0x80000000)
                SectorsRead = ReadSectorsEE(SectorsToRead, ptr);
            else
                SectorsRead = ReadSectors(SectorsToRead, ptr);
            //		M_DEBUG(", Read: %u\n", SectorsRead);

            if (SectorsRead == 0)
                M_DEBUG("StRead: buffer underrun. %u/%lu read.\n", result, sectors);

            result += SectorsRead;
            // if(mode == STMNBLK) break;
            if (mode == 0)
                break;
        }
        *error = sceCdGetError();

        StStartFillStreamBuffer();
    } else {
        result = 0;
    }

    return result;
}
