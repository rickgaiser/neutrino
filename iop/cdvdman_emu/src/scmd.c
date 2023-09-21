/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include "internal.h"

static unsigned char cdvdman_media_changed = 1;

//-------------------------------------------------------------------------
int sceCdReadClock(sceCdCLOCK *rtc)
{
    int rc;

    M_DEBUG("%s(0x%x)\n", __FUNCTION__, rtc);

    cdvdman_stat.err = SCECdErNO;

    rc = cdvdman_sendSCmd(0x08, NULL, 0, (void *)rtc, 8);

    rtc->pad = 0;
    rtc->month &= 0x7f;

    return rc;
}

//-------------------------------------------------------------------------
int sceCdGetDiskType(void)
{
    M_DEBUG("%s() = 0x%02X\n", __FUNCTION__, cdvdman_stat.disc_type_reg);

    return cdvdman_stat.disc_type_reg;
}

//-------------------------------------------------------------------------
int sceCdGetError(void)
{
    if (cdvdman_stat.err != 0)
        M_DEBUG("%s() = %d\n", __FUNCTION__, cdvdman_stat.err);

    return cdvdman_stat.err;
}

//-------------------------------------------------------------------------
int sceCdTrayReq(int mode, u32 *traycnt)
{
    M_DEBUG("%s(%d, 0x%lX)\n", __FUNCTION__, mode, *traycnt);

    if (mode == SCECdTrayCheck) {
        if (traycnt)
            *traycnt = cdvdman_media_changed;

        M_DEBUG("%s TrayCheck result=%d\n", __FUNCTION__, cdvdman_media_changed);

        if (cdvdman_media_changed) {
            DelayThread(4000);
        }

        cdvdman_media_changed = 0;

        return 1;
    }

    // Bit 0 of cdvd status reg is Tray Open.
    // Use it to determine if we are already closed or opened.
    if (mode == SCECdTrayOpen) {
        // Tray is already opened, do nothing.
        if (cdvdman_stat.status & 1) {
            return 0;
        }

        cdvdman_stat.status = SCECdStatShellOpen;
        cdvdman_stat.disc_type_reg = SCECdNODISC;

        DelayThread(11000);

        // So that it reports disc change status.
        cdvdman_media_changed = 1;

        return 1;
    } else if (mode == SCECdTrayClose) {
        // Tray is closed, do nothing.
        if (!(cdvdman_stat.status & 1)) {
            return 0;
        }

        // First state is paused after close.
        cdvdman_stat.status = SCECdStatPause;

        DelayThread(25000);

        // If there is a disc(which for OPL always is) then it will start spinning after a while and detect disc type.

        cdvdman_stat.status = SCECdStatSpin;

        /*
        If the day comes that OPL implements disc swapping, this will be place to reupdate all disc type, LBA start offsets, mediaLsn count and everything else.
        Until then it will the same disc.
        */
        cdvdman_stat.disc_type_reg = cdvdman_settings.media;

        cdvdman_media_changed = 1;

        return 1;
    }

    return 0;
}

//-------------------------------------------------------------------------
int sceCdApplySCmd(u8 cmd, const void *in, u16 in_size, void *out)
{
    M_DEBUG("%s(%d, %d)\n", __FUNCTION__, cmd, in_size);

    return cdvdman_sendSCmd(cmd & 0xff, in, in_size, out, 16);
}

//-------------------------------------------------------------------------
int sceCdStatus(void)
{
    M_DEBUG("%s() = %d\n", __FUNCTION__, (int)cdvdman_stat.status);

    return (int)cdvdman_stat.status;
}

//-------------------------------------------------------------------------
int sceCdBreak_internal(enum ECallSource source)
{
    M_DEBUG("%s() locked = %d\n", __FUNCTION__, sync_flag_locked);

    if (sync_flag_locked)
        return 0;

    cdvdman_stat.status = SCECdStatPause;

    cdvdman_stat.err = SCECdErABRT;

    // Notify external irx that sceCdBreak has finished
    if (source == ECS_EXTERNAL)
        cdvdman_cb_event(SCECdFuncBreak);

    return 1;
}
int sceCdBreak(void)
{
    return sceCdBreak_internal(ECS_EXTERNAL);
}

//-------------------------------------------------------------------------
int sceCdPowerOff(u32 *stat)
{
    M_DEBUG("%s(-)\n", __FUNCTION__);

    return cdvdman_sendSCmd(0x0F, NULL, 0, (unsigned char *)stat, 1);
}

//--------------------------------------------------------------
static int cdvdman_readID(int mode, u8 *buf)
{
    u8 lbuf[16];
    u32 stat;
    int r;

    r = sceCdRI(lbuf, &stat);
    if ((r == 0) || (stat))
        return 0;

    if (mode == 0) { // GUID
        u32 *GUID0 = (u32 *)&buf[0];
        u32 *GUID1 = (u32 *)&buf[4];
        *GUID0 = lbuf[0] | 0x08004600; // Replace the MODEL ID segment with the SCE OUI, to get the console's IEEE1394 EUI-64.
        *GUID1 = *(u32 *)&lbuf[4];
    } else { // ModelID
        u32 *ModelID = (u32 *)&buf[0];
        *ModelID = (*(u32 *)&lbuf[0]) >> 8;
    }

    return 1;
}

//--------------------------------------------------------------
int sceCdReadGUID(u64 *GUID)
{
    M_DEBUG("%s(-)\n", __FUNCTION__);

    return cdvdman_readID(0, (u8 *)GUID);
}

//--------------------------------------------------------------
int sceCdReadModelID(unsigned long int *ModelID)
{
    M_DEBUG("%s(-)\n", __FUNCTION__);

    return cdvdman_readID(1, (u8 *)ModelID);
}

//-------------------------------------------------------------------------
int sceCdReadDvdDualInfo(int *on_dual, u32 *layer1_start)
{
    M_DEBUG("%s(-, -)\n", __FUNCTION__);

    if (cdvdman_settings.flags & IOPCORE_COMPAT_EMU_DVDDL) {
        // Make layer 1 point to layer 0.
        *layer1_start = 0;
        *on_dual = 1;
    } else {
        *layer1_start = cdvdman_settings.layer1_start;
        *on_dual = (cdvdman_settings.layer1_start > 0) ? 1 : 0;
    }

    return 1;
}

//-------------------------------------------------------------------------
int sceCdRI(u8 *buf, u32 *stat)
{
    u8 rdbuf[16];

    M_DEBUG("%s(-, -)\n", __FUNCTION__);

    if (cdvdman_settings.ilink_id_int != 0) {
        rdbuf[0] = 0;
        memcpy(&rdbuf[1], cdvdman_settings.ilink_id, 8);
    } else {
        cdvdman_sendSCmd(0x12, NULL, 0, rdbuf, 9);
    }

    if (stat)
        *stat = (u32)rdbuf[0];

    memcpy((void *)buf, (void *)&rdbuf[1], 8);

    return 1;
}

//-------------------------------------------------------------------------
int sceCdRC(sceCdCLOCK *rtc)
{
    M_DEBUG("%s(-)\n", __FUNCTION__);

    cdvdman_stat.err = SCECdErNO;

    return cdvdman_sendSCmd(0x08, NULL, 0, (void *)rtc, 8);
}

//-------------------------------------------------------------------------
static int cdvdman_readMechaconVersion(u8 *mname, u32 *stat)
{
    u8 rdbuf[16];
    u8 wrbuf[16];

    wrbuf[0] = 0;
    cdvdman_sendSCmd(0x03, wrbuf, 1, rdbuf, 4);

    *stat = rdbuf[0] & 0x80;
    rdbuf[0] &= 0x7f;

    memcpy(mname, &rdbuf[0], 4);

    return 1;
}

//-------------------------------------------------------------------------
int sceCdRM(char *m, u32 *stat)
{
    int r;
    u8 rdbuf[16];
    u8 wrbuf[16];

    M_DEBUG("%s(-, -)\n", __FUNCTION__);

    *stat = 0;
    r = cdvdman_readMechaconVersion(rdbuf, stat);
    if ((r == 1) && (0x104FE < (rdbuf[3] | (rdbuf[2] << 8) | (rdbuf[1] << 16)))) {

        memcpy(&m[0], "M_NAME_UNKNOWN\0\0", 16);
        *stat |= 0x40;
    } else {
        wrbuf[0] = 0;
        cdvdman_sendSCmd(0x17, wrbuf, 1, rdbuf, 9);

        *stat = rdbuf[0];
        memcpy(&m[0], &rdbuf[1], 8);

        wrbuf[0] = 8;
        cdvdman_sendSCmd(0x17, wrbuf, 1, rdbuf, 9);

        *stat |= rdbuf[0];
        memcpy(&m[8], &rdbuf[1], 8);
    }

    return 1;
}
