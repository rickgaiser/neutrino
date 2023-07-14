/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include "internal.h"

//-------------------------------------------------------------------------
int sceCdSync(int mode)
{
    DPRINTF("%s(%d) locked = %d, ic=%d\n", __FUNCTION__, mode, sync_flag_locked, QueryIntrContext());

    if (!sync_flag_locked)
        return 0; // Completed

    if ((mode == 1) || (mode == 17))
        return 1; // Not completed

    while (sync_flag_locked)
        WaitEventFlag(cdvdman_stat.intr_ef, CDVDEF_MAN_UNLOCKED, WEF_AND, NULL);

    return 0; // Completed
}

//-------------------------------------------------------------------------
int sceCdRead(u32 lsn, u32 sectors, void *buf, sceCdRMode *mode)
{
    int result;

    result = sceCdRead_internal(lsn, sectors, buf, NULL, ECS_EXTERNAL);

    if ((result == 1) && (cdvdman_settings.common.flags & IOPCORE_COMPAT_ALT_READ) && !QueryIntrContext())
        WaitEventFlag(cdvdman_stat.intr_ef, CDVDEF_MAN_UNLOCKED, WEF_AND, NULL);

    return result;
}

//-------------------------------------------------------------------------
int sceCdReadCdda(u32 lsn, u32 sectors, void *buf, sceCdRMode *mode)
{
    DPRINTF("%s(%d, %d, %08x, %08x)\n", __FUNCTION__, (int)lsn, (int)sectors, (int)buf, (int)mode);

    return sceCdRead(lsn, sectors, buf, mode);
}

//-------------------------------------------------------------------------
static void lba_to_msf(s32 lba, u8 *m, u8 *s, u8 *f)
{
    lba += 150;
    *m = lba / (60 * 75);
    *s = (lba / 75) % 60;
    *f = lba % 75;
}

//-------------------------------------------------------------------------
typedef struct {
    u8 addr_ctrl;
    u8 track_no;
    u8 index_no;
    u8 reserved[3];
    u8 zero;
    u8 abs_min;
    u8 abs_sec;
    u8 abs_frm;
} __attribute__((packed)) toc_point_t;

typedef struct {
    toc_point_t a0;
    toc_point_t a1;
    toc_point_t a2;
    toc_point_t track[99];
    u32 filler;
} __attribute__((packed)) toc_t;

//-------------------------------------------------------------------------
static int cdvdman_fill_toc(u8 *tocBuff)
{
    u8 discType = cdvdman_stat.disc_type_reg & 0xFF;

    DPRINTF("cdvdman_fill_toc tocBuff=0x%08x discType=0x%02X\n", (int)tocBuff, discType);

    if (tocBuff == NULL) {
        return 0;
    }

    switch (discType) {
        case 0x12: // SCECdPS2CD
        case 0x13: // SCECdPS2CDDA
            toc_t *t = (toc_t *)tocBuff;
            u8 min, sec, frm;

            memset(tocBuff, 0, 1024);

            // Number of FirstTrack,
            // Always 1 until PS2CCDA support get's added.
            t->a0.addr_ctrl = 0x00;
            t->a0.track_no = 0x00;
            t->a0.index_no = 0xA0; // ???
            t->a0.abs_min = itob(1);
            t->a0.abs_min = itob(0);
            t->a0.abs_min = itob(0);

            // Number of LastTrack
            // Always 1 until PS2CCDA support get's added.
            t->a1.addr_ctrl = 0x00;
            t->a1.track_no = 0x00;
            t->a1.index_no = 0xA1; // ???
            t->a1.abs_min = itob(1);
            t->a1.abs_min = itob(0);
            t->a1.abs_min = itob(0);

            // DiskLength
            lba_to_msf(mediaLsnCount, &min, &sec, &frm);
            t->a2.addr_ctrl = 0x00;
            t->a2.track_no = 0x00;
            t->a2.index_no = 0xA2; // ???
            t->a2.abs_min = itob(min);
            t->a2.abs_sec = itob(sec);
            t->a2.abs_frm = itob(frm);

            // Later when PS2CCDA is added the tracks need to get filled in toc too.
            break;

        case 0x14: // SCECdPS2DVD
        case 0xFE: // SCECdDVDV
            // Toc for single layer DVD.
            memset(tocBuff, 0, 2048);

            // Write only what we need, memset has cleared the above buffers.
            //  Single Layer - Values are fixed.
            tocBuff[0] = 0x04;
            tocBuff[1] = 0x02;
            tocBuff[2] = 0xF2;
            tocBuff[4] = 0x86;
            tocBuff[5] = 0x72;

            // These values are fixed on all discs, except position 14 which is the OTP/PTP flags which are 0 in single layer.

            tocBuff[12] = 0x01;
            tocBuff[13] = 0x02;
            tocBuff[14] = 0x01;
            tocBuff[17] = 0x03;

            u32 maxlsn = mediaLsnCount + (0x30000 - 1);
            tocBuff[20] = (maxlsn >> 24) & 0xFF;
            tocBuff[21] = (maxlsn >> 16) & 0xff;
            tocBuff[22] = (maxlsn >> 8) & 0xff;
            tocBuff[23] = (maxlsn >> 0) & 0xff;
            break;

        default:
            // Check if we are DVD9 game and fill toc for it.

            if (!(cdvdman_settings.common.flags & IOPCORE_COMPAT_EMU_DVDDL)) {
                memset(tocBuff, 0, 2048);

                // Dual sided - Values are fixed.
                tocBuff[0] = 0x24;
                tocBuff[1] = 0x02;
                tocBuff[2] = 0xF2;
                tocBuff[4] = 0x41;
                tocBuff[5] = 0x95;

                // These values are fixed on all discs, except position 14 which is the OTP/PTP flags.
                tocBuff[12] = 0x01;
                tocBuff[13] = 0x02;
                tocBuff[14] = 0x21; // PTP
                tocBuff[15] = 0x10;

                // Values are fixed.
                tocBuff[17] = 0x03;

                u32 l1s = mediaLsnCount + 0x30000 - 1;
                tocBuff[20] = (l1s >> 24);
                tocBuff[21] = (l1s >> 16) & 0xff;
                tocBuff[22] = (l1s >> 8) & 0xff;
                tocBuff[23] = (l1s >> 0) & 0xff;

                return 1;
            }

            // Not known type.
            DPRINTF("cdvdman_fill_toc unimplemented for  discType=%02X\n", discType);
            return 0;
    }

    return 1;
}

//-------------------------------------------------------------------------
int sceCdGetToc_internal(u8 *toc, enum ECallSource source)
{
    DPRINTF("%s(-)\n", __FUNCTION__);

    if (sync_flag_locked)
        return 0;

    cdvdman_stat.err = SCECdErNO;
    int result = cdvdman_fill_toc(toc);

    if (!result) {
        cdvdman_stat.err = SCECdErREAD;
    }

    // Notify external irx that sceCdGetToc has finished
    if (source == ECS_EXTERNAL)
        cdvdman_cb_event(SCECdFuncGetToc);

    return result;
}
int sceCdGetToc(u8 *toc)
{
    return sceCdGetToc_internal(toc, ECS_EXTERNAL);
}

//-------------------------------------------------------------------------
int sceCdSeek_internal(u32 lsn, enum ECallSource source)
{
    DPRINTF("%s(%d)\n", __FUNCTION__, (int)lsn);

    if (sync_flag_locked)
        return 0;

    cdvdman_stat.err = SCECdErNO;

    cdvdman_stat.status = SCECdStatPause;

    // Set the invalid parament error in case of trying to seek more than max lsn.
    if (mediaLsnCount) {
        if (lsn >= mediaLsnCount) {
            DPRINTF("cdvdman_searchfile_init mediaLsnCount=%d\n", mediaLsnCount);
            cdvdman_stat.err = SCECdErIPI;
        }
    }

    // Notify external irx that sceCdSeek has finished
    if (source == ECS_EXTERNAL)
        cdvdman_cb_event(SCECdFuncSeek);

    return 1;
}
int sceCdSeek(u32 lsn)
{
    return sceCdSeek_internal(lsn, ECS_EXTERNAL);
}

//-------------------------------------------------------------------------
int sceCdStandby_internal(enum ECallSource source)
{
    DPRINTF("%s()\n", __FUNCTION__);

    cdvdman_stat.err = SCECdErNO;
    cdvdman_stat.status = SCECdStatPause;

    // Notify external irx that sceCdStandby has finished
    if (source == ECS_EXTERNAL)
        cdvdman_cb_event(SCECdFuncStandby);

    return 1;
}
int sceCdStandby(void)
{
    return sceCdStandby_internal(ECS_EXTERNAL);
}

//-------------------------------------------------------------------------
int sceCdStop_internal(enum ECallSource source)
{
    DPRINTF("%s()\n", __FUNCTION__);

    if (sync_flag_locked)
        return 0;

    cdvdman_stat.err = SCECdErNO;

    cdvdman_stat.status = SCECdStatStop;

    // Notify external irx that sceCdStop has finished
    if (source == ECS_EXTERNAL)
        cdvdman_cb_event(SCECdFuncStop);

    return 1;
}
int sceCdStop(void)
{
    return sceCdStop_internal(ECS_EXTERNAL);
}

//-------------------------------------------------------------------------
int sceCdPause_internal(enum ECallSource source)
{
    DPRINTF("%s()\n", __FUNCTION__);

    if (sync_flag_locked)
        return 0;

    cdvdman_stat.err = SCECdErNO;

    cdvdman_stat.status = SCECdStatPause;

    // Notify external irx that sceCdPause has finished
    if (source == ECS_EXTERNAL)
        cdvdman_cb_event(SCECdFuncPause);

    return 1;
}
int sceCdPause(void)
{
    return sceCdPause_internal(ECS_EXTERNAL);
}

//-------------------------------------------------------------------------
int sceCdDiskReady(int mode)
{
    DPRINTF("%s(%d) locked = %d\n", __FUNCTION__, mode, sync_flag_locked);

    cdvdman_stat.err = SCECdErNO;

    if (cdvdman_cdinited) {
        if (mode == 0) {
            while (sync_flag_locked)
                WaitEventFlag(cdvdman_stat.intr_ef, CDVDEF_MAN_UNLOCKED, WEF_AND, NULL);
        }

        if (!sync_flag_locked)
            return DeviceReady();
    }

    return SCECdNotReady;
}

//-------------------------------------------------------------------------
int sceCdReadDiskID(unsigned int *DiskID)
{
    DPRINTF("%s(-)\n", __FUNCTION__);

    int i;
    u8 *p = (u8 *)DiskID;

    for (i = 0; i < 5; i++) {
        if (p[i] != 0)
            break;
    }
    if (i == 5)
        *((u16 *)DiskID) = (u16)0xadde;
    else
        memcpy(DiskID, cdvdman_settings.common.DiscID, 5);

    return 1;
}
