/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include "internal.h"
#include "xmodload.h"
#include "cdvdman_read.h"
#include "libcdvd-common.h"

IRX_ID(MODNAME, 1, 1);

//------------------ Patch Zone ----------------------
struct cdvdman_settings_common cdvdman_settings = {MODULE_SETTINGS_MAGIC};

//----------------------------------------------------
extern struct irx_export_table _exp_cdvdman;
extern struct irx_export_table _exp_cdvdstm;

// internal functions prototypes
static int cdvdman_writeSCmd(u8 cmd, const void *in, u16 in_size, void *out, u16 out_size);
static unsigned int event_alarm_cb(void *args);

struct cdvdman_cb_data
{
    sceCdCBFunc user_cb;
    StmCallback_t stm0_cb;
    int reason;
};

cdvdman_status_t cdvdman_stat;
static struct cdvdman_cb_data cb_data;

static int cdvdman_scmdsema;

volatile unsigned char cdvdman_cdinited = 0;

//-------------------------------------------------------------------------
void cdvdman_init(void)
{
    //M_DEBUG("%s\n", __FUNCTION__);

    if (!cdvdman_cdinited) {
        M_DEBUG("%s - initializing\n", __FUNCTION__);
        cdvdman_stat.err = SCECdErNO;
        cdvdman_cdinited = 1;
    }
}

//-------------------------------------------------------------------------
int sceCdInit(int init_mode)
{
    M_DEBUG("%s(%d)\n", __FUNCTION__, init_mode);

    cdvdman_init();
    return 1;
}

//-------------------------------------------------------------------------
int sceCdMmode(int media)
{
    M_DEBUG("%s(%d)\n", __FUNCTION__, media);
    return 1;
}

//-------------------------------------------------------------------------
u32 sceCdPosToInt(sceCdlLOCCD *p)
{
    u32 result;

    result = ((u32)p->minute >> 4) * 10 + ((u32)p->minute & 0xF);
    result *= 60;
    result += ((u32)p->second >> 4) * 10 + ((u32)p->second & 0xF);
    result *= 75;
    result += ((u32)p->sector >> 4) * 10 + ((u32)p->sector & 0xF);
    result -= 150;

    M_DEBUG("%s({0x%X, 0x%X, 0x%X, 0x%X}) = %d\n", __FUNCTION__, p->minute, p->second, p->sector, p->track, result);

    return result;
}

//-------------------------------------------------------------------------
sceCdlLOCCD *sceCdIntToPos(u32 i, sceCdlLOCCD *p)
{
    u32 sc, se, mi;

    M_DEBUG("%s(%d)\n", __FUNCTION__, i);

    i += 150;
    se = i / 75;
    sc = i - se * 75;
    mi = se / 60;
    se = se - mi * 60;
    p->sector = (sc - (sc / 10) * 10) + (sc / 10) * 16;
    p->second = (se / 10) * 16 + se - (se / 10) * 10;
    p->minute = (mi / 10) * 16 + mi - (mi / 10) * 10;

    return p;
}

//-------------------------------------------------------------------------
StmCallback_t cdvdman_read_set_stm0_callback(StmCallback_t func)
{
    int oldstate;
    StmCallback_t old_func;

    M_DEBUG("%s(0x%X)\n", __FUNCTION__, func);

    if (sceCdSync(1)) {
        M_DEBUG("%s(0x%X) cdvd busy, failed to set callback\n", __FUNCTION__, func);
        return NULL;
    }

    CpuSuspendIntr(&oldstate);

    old_func = cb_data.stm0_cb;
    cb_data.stm0_cb = func;

    CpuResumeIntr(oldstate);

    return old_func;
}

//-------------------------------------------------------------------------
sceCdCBFunc sceCdCallback(sceCdCBFunc func)
{
    int oldstate;
    sceCdCBFunc old_func;

    M_DEBUG("%s(0x%X)\n", __FUNCTION__, func);

    if (sceCdSync(1)) {
        M_DEBUG("%s(0x%X) cdvd busy, failed to set callback\n", __FUNCTION__, func);
        return NULL;
    }

    CpuSuspendIntr(&oldstate);

    old_func = cb_data.user_cb;
    cb_data.user_cb = func;

    CpuResumeIntr(oldstate);

    return old_func;
}

//-------------------------------------------------------------------------
static int cdvdman_writeSCmd(u8 cmd, const void *in, u16 in_size, void *out, u16 out_size)
{
    int i;
    u8 *p;

    WaitSema(cdvdman_scmdsema);

    if (CDVDreg_SDATAIN & 0x80) {
        SignalSema(cdvdman_scmdsema);
        return 0;
    }

    if (!(CDVDreg_SDATAIN & 0x40)) {
        do {
            (void)CDVDreg_SDATAOUT;
        } while (!(CDVDreg_SDATAIN & 0x40));
    }

    if (in_size > 0) {
        for (i = 0; i < in_size; i++) {
            p = (void *)((const u8 *)in + i);
            CDVDreg_SDATAIN = *p;
        }
    }

    CDVDreg_SCOMMAND = cmd;
    (void)CDVDreg_SCOMMAND;

    while (CDVDreg_SDATAIN & 0x80) {
        ;
    }

    i = 0;
    if (!(CDVDreg_SDATAIN & 0x40)) {
        do {
            if (i >= out_size) {
                break;
            }
            p = (void *)((u8 *)out + i);
            *p = CDVDreg_SDATAOUT;
            i++;
        } while (!(CDVDreg_SDATAIN & 0x40));
    }

    if (!(CDVDreg_SDATAIN & 0x40)) {
        do {
            (void)CDVDreg_SDATAOUT;
        } while (!(CDVDreg_SDATAIN & 0x40));
    }

    SignalSema(cdvdman_scmdsema);

    return 1;
}

//--------------------------------------------------------------
int cdvdman_sendSCmd(u8 cmd, const void *in, u16 in_size, void *out, u16 out_size)
{
    int r, retryCount = 0;

retry:

    r = cdvdman_writeSCmd(cmd & 0xff, in, in_size, out, out_size);
    if (r == 0) {
        DelayThread(2000);
        if (++retryCount <= 2500)
            goto retry;
    }

    DelayThread(2000);

    return 1;
}

//--------------------------------------------------------------
void cdvdman_cb_event(int reason)
{
    iop_sys_clock_t callback_delay = {0,0};

    M_DEBUG("    %s(%d) cb=0x%x\n", __FUNCTION__, reason, cb_data.user_cb);

    cb_data.reason = reason;

    ClearEventFlag(cdvdman_stat.intr_ef, ~CDVDEF_CB_DONE);
    SetAlarm(&callback_delay, &event_alarm_cb, &cb_data);
    WaitEventFlag(cdvdman_stat.intr_ef, CDVDEF_CB_DONE, WEF_AND, NULL);

    M_DEBUG("    %s(%d) done\n", __FUNCTION__, reason);
}

//--------------------------------------------------------------
// Fire callback from interrupt context
static unsigned int event_alarm_cb(void *args)
{
    struct cdvdman_cb_data *cb_data = args;

    M_DEBUG("      %s reason=%d cb=0x%x\n", __FUNCTION__, cb_data->reason, cb_data->user_cb);

    // Allow new read to start
    if (cb_data->reason == SCECdFuncRead) {
        sync_flag_locked = 0;
        iSetEventFlag(cdvdman_stat.intr_ef, CDVDEF_MAN_UNLOCKED);
    }

    // Callback user
    // - is likely to start another read
    if (cb_data->user_cb != NULL) {
        cb_data->user_cb(cb_data->reason);
    }

    // Callback streaming (cdvdstm)
    // - is likely to start another read
    if (cb_data->stm0_cb != NULL) {
        cb_data->stm0_cb();
    }

    // Wakeup threads
    iSetEventFlag(cdvdman_stat.intr_ef, CDVDEF_CB_DONE|CDVDEF_STM_DONE);

    M_DEBUG("      %s done\n", __FUNCTION__);

    return 0;
}

//-------------------------------------------------------------------------
static int intrh_cdrom(void *common)
{
    if (CDVDreg_PWOFF & CDL_DATA_RDY)
        CDVDreg_PWOFF = CDL_DATA_RDY;

    if (CDVDreg_PWOFF & CDL_DATA_END) {
        iSetEventFlag(cdvdman_stat.intr_ef, CDVDEF_STM_DONE|CDVDEF_FSV_S596|CDVDEF_POWER_OFF); // Notify FILEIO and CDVDFSV of the power-off event.
    } else
        CDVDreg_PWOFF = CDL_DATA_COMPLETE; // Acknowledge interrupt

    return 1;
}

//-------------------------------------------------------------------------
int _start(int argc, char **argv)
{
    iop_sema_t smp;
    iop_event_t event;

    // Register exports
    RegisterLibraryEntries(&_exp_cdvdman);
    RegisterLibraryEntries(&_exp_cdvdstm);

    // Create SCMD semaphores
    smp.initial = 1;
    smp.max = 1;
    smp.attr = 0;
    smp.option = 0;
    cdvdman_scmdsema = CreateSema(&smp);

    // Create interrupt event flag
    event.attr = EA_MULTI;
    event.option = 0;
    event.bits = CDVDEF_MAN_UNLOCKED;
    cdvdman_stat.intr_ef = CreateEventFlag(&event);

    // start cdvdman threads
    cdvdman_read_init();

    // register cdrom device driver
    cdvdman_initdev();

    // Install interrupt handler
    RegisterIntrHandler(IOP_IRQ_CDVD, 1, &intrh_cdrom, NULL);
    EnableIntr(IOP_IRQ_CDVD);

    // Acknowledge hardware events (e.g. poweroff)
    if (CDVDreg_PWOFF & CDL_DATA_END)
        CDVDreg_PWOFF = CDL_DATA_END;
    if (CDVDreg_PWOFF & CDL_DATA_RDY)
        CDVDreg_PWOFF = CDL_DATA_RDY;

    // init disk type stuff
    cdvdman_stat.err = SCECdErNO;
    cdvdman_stat.disc_type_reg = (int)cdvdman_settings.media;
    M_DEBUG("DiskType=0x%x\n", cdvdman_settings.media);

    return MODULE_RESIDENT_END;
}

//-------------------------------------------------------------------------
int _shutdown(void)
{
    return 0;
}
