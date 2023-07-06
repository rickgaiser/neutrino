/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include "internal.h"
#include "xmodload.h"

#define MODNAME "cdvd_driver"
IRX_ID(MODNAME, 1, 1);

//------------------ Patch Zone ----------------------
struct CDVDMAN_SETTINGS_TYPE cdvdman_settings = {
    CDVDMAN_SETTINGS_DEFAULT_COMMON,
    CDVDMAN_SETTINGS_DEFAULT_DEVICE_SETTINGS};

//----------------------------------------------------
extern struct irx_export_table _exp_cdvdman;
extern struct irx_export_table _exp_cdvdstm;
extern struct irx_export_table _exp_smsutils;

// internal functions prototypes
static int cdvdman_writeSCmd(u8 cmd, const void *in, u16 in_size, void *out, u16 out_size);
static unsigned int event_alarm_cb(void *args);
static void cdvdman_startThreads(void);
static void cdvdman_create_semaphores(void);
static int cdvdman_read(u32 lsn, u32 sectors, void *buf);

struct cdvdman_cb_data
{
    void (*user_cb)(int reason);
    int reason;
};

cdvdman_status_t cdvdman_stat;
static struct cdvdman_cb_data cb_data;

int cdrom_io_sema;
static int cdrom_rthread_sema;
static int cdvdman_scmdsema;
int cdvdman_searchfilesema;
static int cdvdman_ReadingThreadID;

static StmCallback_t Stm0Callback = NULL;
static iop_sys_clock_t gCallbackSysClock;

#define CDVDMAN_MODULE_VERSION 0x225
static int cdvdman_debug_print_flag = 0;

volatile unsigned char sync_flag_locked;
volatile unsigned char cdvdman_cdinited = 0;
static unsigned int ReadPos = 0; /* Current buffer offset in 2048-byte sectors. */

//-------------------------------------------------------------------------
void cdvdman_init(void)
{
    if (!cdvdman_cdinited) {
        cdvdman_stat.err = SCECdErNO;

        cdvdman_fs_init();
        cdvdman_cdinited = 1;
    }
}

int sceCdInit(int init_mode)
{
    DPRINTF("%s()\n", __FUNCTION__);

    cdvdman_init();
    return 1;
}

//-------------------------------------------------------------------------
static unsigned int cdvdemu_read_end_cb(void *arg)
{
    iSetEventFlag(cdvdman_stat.intr_ef, CDVDEF_READ_END);
    return 0;
}

static int cdvdman_read_sectors(u32 lsn, unsigned int sectors, void *buf)
{
    unsigned int SectorsToRead, remaining;
    void *ptr;

    //DPRINTF("cdvdman_read_sectors lsn=%lu sectors=%u buf=%p\n", lsn, sectors, buf);

    cdvdman_stat.err = SCECdErNO;
    for (ptr = buf, remaining = sectors; remaining > 0;) {
        SectorsToRead = remaining;

        if (cdvdman_settings.common.flags & IOPCORE_COMPAT_ACCU_READS) {
            // Limit transfers to a maximum length of 8, with a restricted transfer rate.
            iop_sys_clock_t TargetTime;

            if (SectorsToRead > 8)
                SectorsToRead = 8;

            TargetTime.hi = 0;
            TargetTime.lo = 20460 * SectorsToRead; // approximately 2KB/3600KB/s = 555us required per 2048-byte data sector at 3600KB/s, so 555 * 36.864 = 20460 ticks per sector with a 36.864MHz clock.
            ClearEventFlag(cdvdman_stat.intr_ef, ~CDVDEF_READ_END);
            SetAlarm(&TargetTime, &cdvdemu_read_end_cb, NULL);
        }

        cdvdman_stat.err = DeviceReadSectors(lsn, ptr, SectorsToRead);
        if (cdvdman_stat.err != SCECdErNO) {
            if (cdvdman_settings.common.flags & IOPCORE_COMPAT_ACCU_READS)
                CancelAlarm(&cdvdemu_read_end_cb, NULL);
            break;
        }

        /* PS2LOGO Decryptor algorithm; based on misfire's code (https://github.com/mlafeldt/ps2logo)
           The PS2 logo is stored within the first 12 sectors, scrambled.
           This algorithm exploits the characteristic that the value used for scrambling will be recorded,
           when it is XOR'ed against a black pixel. The first pixel is black, hence the value of the first byte
           was the value used for scrambling. */
        if (lsn < 13) {
            u32 j;
            u8 *logo = (u8 *)ptr;
            static u8 key = 0;
            if (lsn == 0) // First sector? Copy the first byte as the value for unscrambling the logo.
                key = logo[0];
            if (key != 0) {
                for (j = 0; j < (SectorsToRead * 2048); j++) {
                    logo[j] ^= key;
                    logo[j] = (logo[j] << 3) | (logo[j] >> 5);
                }
            }
        }

        ptr = (void *)((u8 *)ptr + (SectorsToRead * 2048));
        remaining -= SectorsToRead;
        lsn += SectorsToRead;
        ReadPos += SectorsToRead * 2048;

        if (cdvdman_settings.common.flags & IOPCORE_COMPAT_ACCU_READS) {
            // Sleep until the required amount of time has been spent.
            WaitEventFlag(cdvdman_stat.intr_ef, CDVDEF_READ_END, WEF_AND, NULL);
        }
    }

    return (cdvdman_stat.err == SCECdErNO ? 0 : 1);
}

static int cdvdman_read(u32 lsn, u32 sectors, void *buf)
{
    cdvdman_stat.status = SCECdStatRead;

    buf = (void *)PHYSADDR(buf);
    cdvdman_read_sectors(lsn, sectors, buf);

    ReadPos = 0; /* Reset the buffer offset indicator. */

    cdvdman_stat.status = SCECdStatPause;

    return 1;
}

//-------------------------------------------------------------------------
u32 sceCdGetReadPos(void)
{
    DPRINTF("%s() = %d\n", __FUNCTION__, ReadPos);

    return ReadPos;
}

int sceCdRead_internal(u32 lsn, u32 sectors, void *buf, sceCdRMode *mode, enum ECallSource source)
{
    static u32 free_prev = 0;
    u32 free;
    int IsIntrContext, OldState;

    IsIntrContext = QueryIntrContext();

    if (mode != NULL)
        DPRINTF("%s(%d, %d, %08x, {%d, %d, %d}, %d) ic=%d\n", __FUNCTION__, (int)lsn, (int)sectors, (int)buf, (int)source, mode->trycount, mode->spindlctrl, mode->datapattern, IsIntrContext);
    else
        DPRINTF("%s(%d, %d, %08x, NULL, %d) ic=%d\n", __FUNCTION__, (int)lsn, (int)sectors, (int)buf, (int)source, IsIntrContext);

    free = QueryTotalFreeMemSize();
    if (free != free_prev) {
        free_prev = free;
        printf("- memory free = %dKiB\n", free / 1024);
    }

    CpuSuspendIntr(&OldState);

    if (sync_flag_locked) {
        CpuResumeIntr(OldState);
        DPRINTF("%s: exiting (sync_flag_locked)...\n", __FUNCTION__);
        return 0;
    }

    if (IsIntrContext)
        iClearEventFlag(cdvdman_stat.intr_ef, ~CDVDEF_MAN_UNLOCKED);
    else
        ClearEventFlag(cdvdman_stat.intr_ef, ~CDVDEF_MAN_UNLOCKED);

    sync_flag_locked = 1;

    cdvdman_stat.cdread_lba = lsn;
    cdvdman_stat.cdread_sectors = sectors;
    cdvdman_stat.cdread_buf = buf;
    cdvdman_stat.source = source;

    CpuResumeIntr(OldState);

    if (IsIntrContext)
        iSignalSema(cdrom_rthread_sema);
    else
        SignalSema(cdrom_rthread_sema);

    return 1;
}

//-------------------------------------------------------------------------
static void cdvdman_initDiskType(void)
{
    cdvdman_stat.err = SCECdErNO;

    cdvdman_stat.disc_type_reg = (int)cdvdman_settings.common.media;
    DPRINTF("DiskType=0x%x\n", cdvdman_settings.common.media);
}

//-------------------------------------------------------------------------
u32 sceCdPosToInt(sceCdlLOCCD *p)
{
    register u32 result;

    DPRINTF("%s({%d, %d, %d})\n", __FUNCTION__, p->minute, p->second, p->sector);

    result = ((u32)p->minute >> 16) * 10 + ((u32)p->minute & 0xF);
    result *= 60;
    result += ((u32)p->second >> 16) * 10 + ((u32)p->second & 0xF);
    result *= 75;
    result += ((u32)p->sector >> 16) * 10 + ((u32)p->sector & 0xF);
    result -= 150;

    return result;
}

//-------------------------------------------------------------------------
sceCdlLOCCD *sceCdIntToPos(u32 i, sceCdlLOCCD *p)
{
    register u32 sc, se, mi;

    DPRINTF("%s(%d)\n", __FUNCTION__, i);

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
sceCdCBFunc sceCdCallback(sceCdCBFunc func)
{
    int oldstate;
    void *old_cb;

    DPRINTF("%s(0x%X)\n", __FUNCTION__, func);

    if (sceCdSync(1))
        return NULL;

    CpuSuspendIntr(&oldstate);

    old_cb = cb_data.user_cb;
    cb_data.user_cb = func;

    CpuResumeIntr(oldstate);

    return old_cb;
}

//-------------------------------------------------------------------------
int sceCdSC(int code, int *param)
{
    int result;

    DPRINTF("%s(0x%X, 0x%X)\n", __FUNCTION__, code, *param);

    switch (code) {
        case CDSC_GET_INTRFLAG:
            result = cdvdman_stat.intr_ef;
            break;
        case CDSC_IO_SEMA:
            if (*param) {
                WaitSema(cdrom_io_sema);
            } else
                SignalSema(cdrom_io_sema);

            result = *param; // EE N-command code.
            break;
        case CDSC_GET_VERSION:
            result = CDVDMAN_MODULE_VERSION;
            break;
        case CDSC_GET_DEBUG_STATUS:
            *param = (int)&cdvdman_debug_print_flag;
            result = 0xFF;
            break;
        case CDSC_SET_ERROR:
            result = cdvdman_stat.err = *param;
            break;
        default:
            result = 1; // dummy result
    }

    return result;
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
    if (cb_data.user_cb != NULL) {
        cb_data.reason = reason;

        ClearEventFlag(cdvdman_stat.intr_ef, ~CDVDEF_CB_DONE);
        SetAlarm(&gCallbackSysClock, &event_alarm_cb, &cb_data);
        WaitEventFlag(cdvdman_stat.intr_ef, CDVDEF_CB_DONE, WEF_AND, NULL);
    }
}

//--------------------------------------------------------------
static unsigned int event_alarm_cb(void *args)
{
    struct cdvdman_cb_data *cb_data = args;

    if (cb_data->user_cb != NULL) // This interrupt does not occur immediately, hence check for the callback again here.
        cb_data->user_cb(cb_data->reason);

    iSetEventFlag(cdvdman_stat.intr_ef, CDVDEF_CB_DONE);

    return 0;
}

//--------------------------------------------------------------
static void cdvdman_cdread_Thread(void *args)
{
    while (1) {
        WaitSema(cdrom_rthread_sema);

        DPRINTF("%s() [%d, %d, %08x, %d]\n", __FUNCTION__, (int)cdvdman_stat.cdread_lba, (int)cdvdman_stat.cdread_sectors, (int)cdvdman_stat.cdread_buf, (int)cdvdman_stat.source);

        cdvdman_read(cdvdman_stat.cdread_lba, cdvdman_stat.cdread_sectors, cdvdman_stat.cdread_buf);

        sync_flag_locked = 0;
        SetEventFlag(cdvdman_stat.intr_ef, CDVDEF_MAN_UNLOCKED);

        switch (cdvdman_stat.source) {
            case ECS_EXTERNAL:
                // Call from external irx (via sceCdRead)

                // Notify external irx that sceCdRead has finished
                cdvdman_cb_event(SCECdFuncRead);
                break;
            case ECS_SEARCHFILE:
            case ECS_EE_RPC:
                // Call from searchfile and ioops
                break;
            case ECS_STREAMING:
                // Call from streaming

                // The event will trigger the transmission of data to EE
                SetEventFlag(cdvdman_stat.intr_ef, CDVDEF_STM_DONE);

                // The callback will trigger a new read (if needed)
                if (Stm0Callback != NULL)
                    Stm0Callback();
                break;
        }
    }
}

//-------------------------------------------------------------------------
static void cdvdman_startThreads(void)
{
    iop_thread_t thread_param;

    cdvdman_stat.status = SCECdStatPause;
    cdvdman_stat.err = SCECdErNO;

    thread_param.thread = &cdvdman_cdread_Thread;
    thread_param.stacksize = 0x1000;
    thread_param.priority = 0x0f;
    thread_param.attr = TH_C;
    thread_param.option = 0xABCD0000;

    cdvdman_ReadingThreadID = CreateThread(&thread_param);
    StartThread(cdvdman_ReadingThreadID, NULL);
}

//-------------------------------------------------------------------------
static void cdvdman_create_semaphores(void)
{
    iop_sema_t smp;

    smp.initial = 1;
    smp.max = 1;
    smp.attr = 0;
    smp.option = 0;

    cdvdman_scmdsema = CreateSema(&smp);
    smp.initial = 0;
    cdrom_rthread_sema = CreateSema(&smp);
}

//-------------------------------------------------------------------------
static int intrh_cdrom(void *common)
{
    if (CDVDreg_PWOFF & CDL_DATA_RDY)
        CDVDreg_PWOFF = CDL_DATA_RDY;

    if (CDVDreg_PWOFF & CDL_DATA_END) {
        // If IGR is enabled: Do not acknowledge the interrupt here. The EE-side IGR code will monitor and acknowledge it.
        if (cdvdman_settings.common.flags & IOPCORE_ENABLE_POFF) {
            CDVDreg_PWOFF = CDL_DATA_END; // Acknowldge power-off request.
        }
        iSetEventFlag(cdvdman_stat.intr_ef, CDVDEF_STM_DONE|CDVDEF_FSV_S596|CDVDEF_POWER_OFF); // Notify FILEIO and CDVDFSV of the power-off event.
    } else
        CDVDreg_PWOFF = CDL_DATA_COMPLETE; // Acknowledge interrupt

    return 1;
}

static inline void InstallIntrHandler(void)
{
    RegisterIntrHandler(IOP_IRQ_CDVD, 1, &intrh_cdrom, NULL);
    EnableIntr(IOP_IRQ_CDVD);

    // Acknowledge hardware events (e.g. poweroff)
    if (CDVDreg_PWOFF & CDL_DATA_END)
        CDVDreg_PWOFF = CDL_DATA_END;
    if (CDVDreg_PWOFF & CDL_DATA_RDY)
        CDVDreg_PWOFF = CDL_DATA_RDY;
}

int _start(int argc, char **argv)
{
    // register exports
    RegisterLibraryEntries(&_exp_cdvdman);
    RegisterLibraryEntries(&_exp_cdvdstm);
    RegisterLibraryEntries(&_exp_smsutils);

    DeviceInit();

    // Setup the callback timer.
    USec2SysClock((cdvdman_settings.common.flags & IOPCORE_COMPAT_ACCU_READS) ? 5000 : 0, &gCallbackSysClock);

    // create SCMD/searchfile semaphores
    cdvdman_create_semaphores();

    // start cdvdman threads
    cdvdman_startThreads();

    // register cdrom device driver
    cdvdman_initdev();
    InstallIntrHandler();

    // hook MODLOAD's exports
    hookMODLOAD();

    // init disk type stuff
    cdvdman_initDiskType();

    return MODULE_RESIDENT_END;
}

//-------------------------------------------------------------------------
void SetStm0Callback(StmCallback_t callback)
{
    Stm0Callback = callback;
}

//-------------------------------------------------------------------------
int _shutdown(void)
{
    DeviceDeinit();

    return 0;
}
