/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include "internal.h"

//-------------------------------------------------------------------------
int sceCdSync(int mode)
{
    M_DEBUG("%s(%d) locked = %d, ic=%d\n", __FUNCTION__, mode, sync_flag_locked, QueryIntrContext());

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

    result = sceCdRead_internal(lsn, sectors, buf, mode, ECS_EXTERNAL);

    if ((result == 1) && (cdvdman_settings.flags & IOPCORE_COMPAT_ALT_READ) && !QueryIntrContext())
        WaitEventFlag(cdvdman_stat.intr_ef, CDVDEF_MAN_UNLOCKED, WEF_AND, NULL);

    return result;
}

//-------------------------------------------------------------------------
int sceCdReadCdda(u32 lsn, u32 sectors, void *buf, sceCdRMode *mode)
{
    M_DEBUG("%s(%d, %d, %08x, %08x)\n", __FUNCTION__, (int)lsn, (int)sectors, (int)buf, (int)mode);

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
    u8 ctrl_adr;
    u8 track_no;
    u8 point;
    u8 min;
    u8 sec;
    u8 frm;
    u8 zero; // always zero
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

    M_DEBUG("cdvdman_fill_toc tocBuff=%08x discType=%02X\n", (int)tocBuff, discType);

    if (tocBuff == NULL) {
        return 0;
    }

    switch (discType) {
        case SCECdPS2CD:
        case SCECdPS2CDDA:
            toc_t *t = (toc_t *)tocBuff;
            u8 min, sec, frm;

            memset(tocBuff, 0, sizeof(toc_t));

            // source: http://www.13thmonkey.org/documentation/SCSI/mmc2r11a.pdf, p. 17
            // also table 308, p.230
            // control field = 4 - Data track, recorded uninterrupted, digital copy prohibited
            // ADR = 1 (Mode-1 Q)
            t->a0.ctrl_adr = 0x41;
            t->a0.track_no = 0x00; // always 0 for lead-in

            // Number of First Track
            t->a0.point = 0xA0;
            t->a0.min = t->a0.sec = t->a0.frm = 0;
            t->a0.abs_min = 1; // Number of First Track
            t->a0.abs_sec = 0; // 0 - CD-DA or CD-ROM
            t->a0.abs_frm = 0;

            // Number of Last Track
            t->a1.ctrl_adr = 0; // ?? should be 0x41
            t->a1.track_no = 0;
            t->a1.point = 0xA1;
            t->a1.min = t->a1.sec = t->a1.frm = 0;
            t->a1.abs_min = 1; // Number of Last Track, always 1 until PS2CCDA support get's added.
            t->a1.abs_sec = 0;
            t->a1.abs_frm = 0;

            // Disk Length
            t->a2.ctrl_adr = 0; // ?? should be 0x41
            t->a2.track_no = 0;
            t->a2.point = 0xA2;
            t->a2.min = t->a2.sec = t->a2.frm = 0;
            lba_to_msf(mediaLsnCount, &min, &sec, &frm);
            t->a2.abs_min = itob(min);
            t->a2.abs_sec = itob(sec);
            t->a2.abs_frm = itob(frm);

            // ?? do we need to show here also first data track ??
            // t->track[0].point = 0x01;

            // Later if PS2CCDA is added the tracks need to get filled in toc too.
            break;

        case SCECdPS2DVD:
        case SCECdDVDV:
            // Toc for single layer DVD.
            memset(tocBuff, 0, 2048);

            u8 dual = 0;
            if ((!(cdvdman_settings.flags & IOPCORE_COMPAT_EMU_DVDDL)) || (cdvdman_settings.layer1_start > 0))
                dual = 1;

            // Write only what we need, memset has cleared the above buffers.
            //  Single Layer - Values are fixed.
            tocBuff[0] = dual ? 0x24 : 0x04;
            tocBuff[1] = 0x02;
            tocBuff[2] = 0xF2;
            tocBuff[3] = 0x00;

            tocBuff[4] = dual ? 0x41 : 0x86;
            tocBuff[5] = dual ? 0x95 : 0x72;

            // These values are fixed on all discs, except position 14 which is the OTP/PTP flags which are 0 in single layer.

            tocBuff[12] = 0x01;
            tocBuff[13] = 0x02;
            tocBuff[14] = dual ? 0x21 : 0x01; // OTP/PTP flag
            tocBuff[15] = 0x00;

            tocBuff[17] = 0x03;

            u32 maxlsn = (cdvdman_settings.layer1_start ? cdvdman_settings.layer1_start : mediaLsnCount) + (0x30000 - 1);
            tocBuff[20] = (maxlsn >> 24) & 0xFF;
            tocBuff[21] = (maxlsn >> 16) & 0xff;
            tocBuff[22] = (maxlsn >> 8) & 0xff;
            tocBuff[23] = (maxlsn >> 0) & 0xff;
            break;

        default:
            // Not known type.
            M_DEBUG("cdvdman_fill_toc unimplemented for discType=%02X\n", discType);
            return 0;
    }

    return 1;
}

//-------------------------------------------------------------------------
int sceCdGetToc_internal(u8 *toc, enum ECallSource source)
{
    M_DEBUG("%s(-)\n", __FUNCTION__);

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
    M_DEBUG("%s(%d)\n", __FUNCTION__, (int)lsn);

    if (sync_flag_locked)
        return 0;

    cdvdman_stat.err = SCECdErNO;

    cdvdman_stat.status = SCECdStatPause;

    // Set the invalid parament error in case of trying to seek more than max lsn.
    if (mediaLsnCount) {
        if (lsn >= mediaLsnCount) {
            M_DEBUG("cdvdman_searchfile_init mediaLsnCount=%d\n", mediaLsnCount);
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
    M_DEBUG("%s()\n", __FUNCTION__);

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
    M_DEBUG("%s()\n", __FUNCTION__);

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
    M_DEBUG("%s()\n", __FUNCTION__);

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
    M_DEBUG("%s(%d) locked = %d\n", __FUNCTION__, mode, sync_flag_locked);

    cdvdman_stat.err = SCECdErNO;

    if (cdvdman_cdinited) {
        if (mode == 0) {
            while (sync_flag_locked)
                WaitEventFlag(cdvdman_stat.intr_ef, CDVDEF_MAN_UNLOCKED, WEF_AND, NULL);
        }

        if (!sync_flag_locked)
            return SCECdComplete;
    }

    return SCECdNotReady;
}

//-------------------------------------------------------------------------
int sceCdReadDiskID(unsigned int *DiskID)
{
    M_DEBUG("%s(-)\n", __FUNCTION__);

    int i;
    u8 *p = (u8 *)DiskID;

    for (i = 0; i < 5; i++) {
        if (p[i] != 0)
            break;
    }
    if (i == 5)
        *((u16 *)DiskID) = (u16)0xadde;
    else
        memcpy(DiskID, cdvdman_settings.disk_id, 5);

    return 1;
}
