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

    DPRINTF("%s(-)\n", __FUNCTION__);

    cdvdman_stat.err = SCECdErNO;

    rc = cdvdman_sendSCmd(0x08, NULL, 0, (void *)rtc, 8);

    rtc->pad = 0;
    rtc->month &= 0x7f;

    return rc;
}

//-------------------------------------------------------------------------
int sceCdGetDiskType(void)
{
    DPRINTF("%s() = %d\n", __FUNCTION__, cdvdman_stat.disc_type_reg);

    return cdvdman_stat.disc_type_reg;
}

//-------------------------------------------------------------------------
int sceCdGetError(void)
{
    if (cdvdman_stat.err != 0)
        DPRINTF("%s() = %d\n", __FUNCTION__, cdvdman_stat.err);

    return cdvdman_stat.err;
}

//-------------------------------------------------------------------------
int sceCdTrayReq(int mode, u32 *traycnt)
{
    DPRINTF("%s(%d, 0x%lX)\n", __FUNCTION__, mode, *traycnt);

    if (mode == SCECdTrayCheck) {
        if (traycnt)
            *traycnt = cdvdman_media_changed;

        cdvdman_media_changed = 0;

        return 1;
    }

    if (mode == SCECdTrayOpen) {
        cdvdman_stat.status = SCECdStatShellOpen;
        cdvdman_stat.disc_type_reg = 0;

        DelayThread(11000);

        cdvdman_stat.err = SCECdErOPENS; /* not sure about this error code */

        return 1;
    } else if (mode == SCECdTrayClose) {
        DelayThread(25000);

        cdvdman_stat.status = SCECdStatPause; /* not sure if the status is right, may be - SCECdStatSpin */
        cdvdman_stat.err = SCECdErNO;         /* not sure if this error code is suitable here */
        cdvdman_stat.disc_type_reg = (int)cdvdman_settings.common.media;

        cdvdman_media_changed = 1;

        return 1;
    }

    return 0;
}

//-------------------------------------------------------------------------
int sceCdApplySCmd(u8 cmd, const void *in, u16 in_size, void *out)
{
    DPRINTF("%s(%d, %d)\n", __FUNCTION__, cmd, in_size);

    return cdvdman_sendSCmd(cmd & 0xff, in, in_size, out, 16);
}

//-------------------------------------------------------------------------
int sceCdStatus(void)
{
    DPRINTF("%s() = %d\n", __FUNCTION__, cdvdman_stat.status);

    return cdvdman_stat.status;
}

//-------------------------------------------------------------------------
int sceCdBreak_internal(enum ECallSource source)
{
    DPRINTF("%s() locked = %d\n", __FUNCTION__, sync_flag_locked);

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
    DPRINTF("%s(-)\n", __FUNCTION__);

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
    DPRINTF("%s(-)\n", __FUNCTION__);

    return cdvdman_readID(0, (u8 *)GUID);
}

//--------------------------------------------------------------
int sceCdReadModelID(unsigned long int *ModelID)
{
    DPRINTF("%s(-)\n", __FUNCTION__);

    return cdvdman_readID(1, (u8 *)ModelID);
}

//-------------------------------------------------------------------------
int sceCdReadDvdDualInfo(int *on_dual, u32 *layer1_start)
{
    DPRINTF("%s(-, -)\n", __FUNCTION__);

    if (cdvdman_settings.common.flags & IOPCORE_COMPAT_EMU_DVDDL) {
        // Make layer 1 point to layer 0.
        *layer1_start = 0;
        *on_dual = 1;
    } else {
        *layer1_start = cdvdman_settings.common.layer1_start;
        *on_dual = (cdvdman_settings.common.layer1_start > 0) ? 1 : 0;
    }

    return 1;
}

//-------------------------------------------------------------------------
int sceCdRI(u8 *buf, u32 *stat)
{
    u8 rdbuf[16];

    DPRINTF("%s(-, -)\n", __FUNCTION__);

    cdvdman_sendSCmd(0x12, NULL, 0, rdbuf, 9);

    if (stat)
        *stat = (u32)rdbuf[0];

    memcpy((void *)buf, (void *)&rdbuf[1], 8);

    return 1;
}

//-------------------------------------------------------------------------
int sceCdRC(sceCdCLOCK *rtc)
{
    DPRINTF("%s(-)\n", __FUNCTION__);

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
    register int r;
    u8 rdbuf[16];
    u8 wrbuf[16];

    DPRINTF("%s(-, -)\n", __FUNCTION__);

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
