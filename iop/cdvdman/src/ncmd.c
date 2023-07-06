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
int sceCdGetToc_internal(u8 *toc, enum ECallSource source)
{
    DPRINTF("%s(-) !!! function not supported !!!\n", __FUNCTION__);

    if (sync_flag_locked)
        return 0;

    cdvdman_stat.err = SCECdErREAD;

    return 0; // Not supported
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
