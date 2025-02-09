#include <xthbase.h>
#include <thevent.h>
#include <thsemap.h>

#include "internal.h"
#include "device.h"
#include "cdvdman_read.h"


static int cdrom_rthread_sema;
static StmCallback_t Stm0Callback = NULL;
static unsigned int ReadPos = 0; /* Current buffer offset in 2048-byte sectors. */
static int cdvdman_ReadingThreadID;
volatile unsigned char sync_flag_locked;


//-------------------------------------------------------------------------
static unsigned int cdvdman_read_sectors_end_cb(void *arg)
{
    iSetEventFlag(cdvdman_stat.intr_ef, CDVDEF_READ_END);
    return 0;
}

//-------------------------------------------------------------------------
// Calculate and return the time it takes to read these sectors
static u32 accurate_read_clocks(u32 lsn, unsigned int sectors)
{
    u32 usec_per_sector;
    // Read-ahead tracking
    static u64 clock_last_read_end; // clock at which the next read will start reading into the read-ahead buffer
    static u32 next_read_lsn; // lsn of the next read into the read-ahead buffer

    /*
     * Limit transfer speed to match the physical drive in the ps2
     *
     * Base read speeds:
     * -  1x =  150KiB/s =   75 sectors/s for CD
     * -  1x = 1350KiB/s =  675 sectors/s for DVD
     *
     * Maximum read speeds:
     * - 24x = 3600KiB/s = 1900 sectors/s for CD
     * -  4x = 5400KiB/s = 2700 sectors/s for DVD
     *
     * CLV read speed is constant (Maximum / 2.4):
     * - 10.00x = 1500KiB/s for CD
     * -  1.67x = 2250KiB/s for DVD
     *
     * CAV read speed is:
     * - Same as CLV at the inner sectors
     * - Same as max at the outer sectors
     *
     * Sony documentation states only CAV is used.
     * But there is some discussion about if this is true 100% of the time.
     */

    if (cdvdman_settings.media == 0x12) {
        // CD constant values
        // ------------------
        // 2 KiB
        // 1000000 us / s
        // 333000 sectors per CD
        // 1500 KiB/s inner speed (10X)
        // 3600 KiB/s outer speed (24X)
        const u32 cd_const_1 = (1000000 * 2 * 333000ll) / (3600 - 1500);
        const u32 cd_const_2 = (       1500 * 333000ll) / (3600 - 1500);
        usec_per_sector = cd_const_1 / (cd_const_2 + lsn);
        // CD is limited to 3000KiB/s = 667us / sector
        // Compensation: our code seems 23us / sector slower than sony CDVD
        if (usec_per_sector < (667-23))
            usec_per_sector = (667-23);
    } else if (cdvdman_settings.layer1_start != 0) {
        // DVD dual layer constant values
        // ------------------------------
        // 2 KiB
        // 1000000 us / s
        // 2084960 sectors per DVD (8.5GB/2)
        // 2250 KiB/s inner speed (1.67X)
        // 5400 KiB/s outer speed (4X)
        const u32 dvd_dl_const_1 = (1000000 * 2 * 2084960ll) / (5400 - 2250);
        const u32 dvd_dl_const_2 = (       2250 * 2084960ll) / (5400 - 2250);
        // For dual layer DVD, the second layer starts at 0
        // PS2 uses PTP = Parallel Track Path
        u32 effective_lsn = lsn;
        if (effective_lsn >= cdvdman_settings.layer1_start)
            effective_lsn -= cdvdman_settings.layer1_start;

        usec_per_sector = dvd_dl_const_1 / (dvd_dl_const_2 + effective_lsn);
    }
    else {
        // DVD single layer constant values
        // --------------------------------
        // 2 KiB
        // 1000000 us / s
        // 2298496 sectors per DVD (4.7GB)
        // 2250 KiB/s inner speed (1.67X)
        // 5400 KiB/s outer speed (4X)
        const u32 dvd_sl_const_1 = (1000000 * 2 * 2298496ll) / (5400 - 2250);
        const u32 dvd_sl_const_2 = (       2250 * 2298496ll) / (5400 - 2250);
        usec_per_sector = dvd_sl_const_1 / (dvd_sl_const_2 + lsn);
    }

    M_DEBUG("    Sector %lu (%u sectors) CAV usec_per_sector = %d\n", lsn, sectors, usec_per_sector);

    // Delay clocks for the actual read
    u32 clocks_delay = usec_per_sector * sectors * 37; // 36.864 MHz

    // Current system clock @ 36.864 MHz
    iop_sys_clock_t sysclock;
    GetSystemTime(&sysclock);
    // Convert to easy to use u64 value
    // NOTE that using u64 is EXPENSIVE becouse the IOP is only 32 bit
    u64 clock_now = (u64)sysclock.hi << 32 | sysclock.lo;
    // First time initialization
    if (clock_last_read_end == 0)
        clock_last_read_end = clock_now;

    if (lsn == next_read_lsn) {
        // Reading consecutive sectors
        // These sectors can be read into the read-ahead buffer of the DVD drive
        // Size = 16 sectors = 32KiB
        // source: https://github.com/PCSX2/pcsx2/blob/master/pcsx2/CDVD/CDVD.cpp

        // How much has been read-ahead
        u32 clocks_read_ahead = clock_now - clock_last_read_end;
        // Limit to 16 sectors
        if (clocks_read_ahead > (usec_per_sector * 37 * 16)) { // 36.864 MHz
            clocks_read_ahead = usec_per_sector * 37 * 16; // 36.864 MHz
        }

        if (clocks_delay < clocks_read_ahead) {
            // All sectors have already been read
            clocks_delay = 0;
            //M_PRINTF("clocks to 0\n");
        } else {
            // Some sectors have been read-ahead
            //M_PRINTF("clocks %d - %d\n", clocks_delay, clocks_read_ahead);
            clocks_delay -= clocks_read_ahead;
            clock_last_read_end = clock_now + clocks_delay;
        }
    } else {
        // Reading random sectors
        // Add seek time?
    }

    next_read_lsn = lsn + sectors;
    clock_last_read_end = clock_now + clocks_delay;

    return clocks_delay;
}

//-------------------------------------------------------------------------
static void cdvdman_read_sectors(u32 lsn, unsigned int sectors, void *buf)
{
    unsigned int remaining;
    void *ptr;
    int endOfMedia = 0;
    u32 clocks_delay = 0;

    M_DEBUG("    %s lsn=%lu sectors=%u buf=%p\n", __FUNCTION__, lsn, sectors, buf);

    if (mediaLsnCount) {

        // If lsn to read is already bigger error already.
        if (lsn >= mediaLsnCount) {
            M_DEBUG("    %s eom lsn=%d sectors=%d leftsectors=%d MaxLsn=%d \n", __FUNCTION__, lsn, sectors, mediaLsnCount - lsn, mediaLsnCount);
            cdvdman_stat.err = SCECdErIPI;
            return;
        }

        // As per PS2 mecha code continue to read what you can and then signal end of media error.
        if ((lsn + sectors) > mediaLsnCount) {
            M_DEBUG("    %s eom lsn=%d sectors=%d leftsectors=%d MaxLsn=%d \n", __FUNCTION__, lsn, sectors, mediaLsnCount - lsn, mediaLsnCount);
            endOfMedia = 1;
            // Limit how much sectors we can read.
            sectors = mediaLsnCount - lsn;
        }
    }

    if ((cdvdman_settings.flags & CDVDMAN_COMPAT_FAST_READS) == 0) {
        // Get the number of clocks delay per sector
        clocks_delay = accurate_read_clocks(lsn, sectors) / sectors;
    }

    cdvdman_stat.err = SCECdErNO;
    for (ptr = buf, remaining = sectors; remaining > 0;) {
        unsigned int SectorsToRead = remaining;

        if (clocks_delay > 0) {
            // Limit transfers to a maximum length of 8, with a restricted transfer rate.
            iop_sys_clock_t TargetTime;

            if (SectorsToRead > 8)
                SectorsToRead = 8;

            TargetTime.hi = 0;
            TargetTime.lo = clocks_delay * SectorsToRead;
            ClearEventFlag(cdvdman_stat.intr_ef, ~CDVDEF_READ_END);
            SetAlarm(&TargetTime, &cdvdman_read_sectors_end_cb, NULL);
        }

        cdvdman_stat.err = DeviceReadSectors(lsn, ptr, SectorsToRead);
        if (cdvdman_stat.err != SCECdErNO) {
            if (clocks_delay > 0)
                CancelAlarm(&cdvdman_read_sectors_end_cb, NULL);
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

        if (clocks_delay > 0) {
            // Sleep until the required amount of time has been spent.
            WaitEventFlag(cdvdman_stat.intr_ef, CDVDEF_READ_END, WEF_AND, NULL);
        }
    }

    // If we had a read that went past the end of media, after reading what we can, set the end of media error.
    if (endOfMedia) {
        cdvdman_stat.err = SCECdErEOM;
    }
}

//-------------------------------------------------------------------------
// Same as cdvdman_read_sectors, but uses a bounce buffer for alignment
static void cdvdman_read_sectors_bounce(u32 lsn, u32 sectors, u16 sector_size, void *buf)
{
    // OPL only has 2048 bytes no matter what. For other sizes we have to copy to the offset and prepoluate the sector header data (the extra bytes.)
    u32 offset = 0;

    if (sector_size == 2340)
        offset = 12; // head - sub - data(2048) -- edc-ecc

    buf = (void *)PHYSADDR(buf);

    // For transfers to unaligned buffers, a double-copy is required to avoid stalling the device's DMA channel.
    WaitSema(cdvdman_searchfilesema);

    u32 nsectors, nbytes;
    u32 rpos = lsn;

    while (sectors > 0) {
        nsectors = sectors;
        if (nsectors > CDVDMAN_BUF_SECTORS)
            nsectors = CDVDMAN_BUF_SECTORS;

        // For other sizes we can only read one sector at a time.
        // There are only very few games (CDDA games, EA Tiburon) that will be affected
        if (sector_size != 2048)
            nsectors = 1;

        cdvdman_read_sectors(rpos, nsectors, cdvdman_buf);

        rpos += nsectors;
        sectors -= nsectors;
        nbytes = nsectors * sector_size;


        // Copy the data for buffer.
        // For any sector other than 2048 one sector at a time is copied.
        memcpy((void *)((u32)buf + offset), cdvdman_buf, nbytes);

        // For these custom sizes we need to manually fix the header.
        // For 2340 we have 12bytes. 4 are position.
        if (sector_size == 2340) {
            u8 *header = (u8 *)buf;
            // position.
            sceCdlLOCCD p;
            sceCdIntToPos(rpos - 1, &p); // to get current pos.
            header[0] = p.minute;
            header[1] = p.second;
            header[2] = p.sector;
            header[3] = 0; // p.track for cdda only non-zero

            // Subheader and copy of subheader.
            header[4] = header[8] = 0;
            header[5] = header[9] = 0;
            header[6] = header[10] = 0x8;
            header[7] = header[11] = 0;
        }

        buf = (void *)((u8 *)buf + nbytes);
    }

    SignalSema(cdvdman_searchfilesema);
}

//--------------------------------------------------------------
static void cdvdman_read_thread(void *args)
{
    cdvdman_read_t req;

    while (1) {
        WaitSema(cdrom_rthread_sema);
        memcpy(&req, &cdvdman_stat.req, sizeof(req));

        M_DEBUG("  %s() [%d, %d, %d, %08x, %d]\n", __FUNCTION__, (int)req.lba, (int)req.sectors, (int)req.sector_size, (int)req.buf, (int)req.source);

        cdvdman_stat.status = SCECdStatRead;
        if (((u32)(req.buf)&3) || (req.sector_size != 2048))
            cdvdman_read_sectors_bounce(req.lba, req.sectors, req.sector_size, req.buf);
        else
            cdvdman_read_sectors(req.lba, req.sectors, req.buf);
        ReadPos = 0; /* Reset the buffer offset indicator. */
        cdvdman_stat.status = SCECdStatPause;

        M_DEBUG("  %s() read done, unlock and callback...\n", __FUNCTION__);

        sync_flag_locked = 0;
        SetEventFlag(cdvdman_stat.intr_ef, CDVDEF_MAN_UNLOCKED);

        switch (req.source) {
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

                if (cdvdman_settings.flags & CDVDMAN_COMPAT_F1_2001)
                    cdvdman_cb_event(SCECdFuncRead);

                break;
        }

        M_DEBUG("  %s() done\n", __FUNCTION__);
    }
}

//-------------------------------------------------------------------------
int sceCdRead_internal(u32 lsn, u32 sectors, void *buf, sceCdRMode *mode, enum ECallSource source)
{
#ifdef DEBUG
    static u32 free_prev = 0;
    u32 free;
#endif
    int OldState;
    u16 sector_size = 2048;

    int intct = QueryIntrContext();

#ifdef DEBUG
    if (mode != NULL)
        M_DEBUG("%s(%d, %d, %08x, {%d, %d, %d}, %d) ic=%d\n", __FUNCTION__, (int)lsn, (int)sectors, (int)buf, mode->trycount, mode->spindlctrl, mode->datapattern, (int)source, intct);
    else
        M_DEBUG("%s(%d, %d, %08x, NULL, %d) ic=%d\n", __FUNCTION__, (int)lsn, (int)sectors, (int)buf, (int)source, intct);
#endif

    // Is is NULL in our emulated cdvdman routines so check if valid.
    if (mode) {
        // 0 is 2048
        if (mode->datapattern == SCECdSecS2328)
            sector_size = 2328;

        if (mode->datapattern == SCECdSecS2340)
            sector_size = 2340;
    }

#ifdef DEBUG
    free = QueryTotalFreeMemSize();
    if (free != free_prev) {
        free_prev = free;
        M_PRINTF("- memory free = %dKiB\n", free / 1024);
    }
#endif

    //
    // Atomically add a new read request
    //
    CpuSuspendIntr(&OldState);
    {
        if (sync_flag_locked) {
            CpuResumeIntr(OldState);
            M_DEBUG("%s: exiting (sync_flag_locked)...\n", __FUNCTION__);
            return 0;
        }

        if (intct)
            iClearEventFlag(cdvdman_stat.intr_ef, ~CDVDEF_MAN_UNLOCKED);
        else
            ClearEventFlag(cdvdman_stat.intr_ef, ~CDVDEF_MAN_UNLOCKED);

        sync_flag_locked = 1;

        cdvdman_stat.req.lba = lsn;
        cdvdman_stat.req.sectors = sectors;
        cdvdman_stat.req.sector_size = sector_size;
        cdvdman_stat.req.buf = buf;
        cdvdman_stat.req.source = source;
    }
    CpuResumeIntr(OldState);

    //
    // Wake up read thread for processing the read request
    //
    M_DEBUG("%s: waking up thread...\n", __FUNCTION__);
    if (intct)
        iSignalSema(cdrom_rthread_sema);
    else
        SignalSema(cdrom_rthread_sema);
    M_DEBUG("%s: waking up thread...done\n", __FUNCTION__);

    return 1;
}

//-------------------------------------------------------------------------
void cdvdman_read_init()
{
    iop_thread_t thread_param;
    iop_sema_t smp;

    smp.initial = 0;
    smp.max = 1;
    smp.attr = 0;
    smp.option = 0;
    cdrom_rthread_sema = CreateSema(&smp);

    cdvdman_stat.status = SCECdStatPause;
    cdvdman_stat.err = SCECdErNO;

    thread_param.thread = &cdvdman_read_thread;
    thread_param.stacksize = 0x1000;
    thread_param.priority = 8;
    thread_param.attr = TH_C;
    thread_param.option = 0xABCD0000;
    cdvdman_ReadingThreadID = CreateThread(&thread_param);
    StartThread(cdvdman_ReadingThreadID, NULL);
}

//-------------------------------------------------------------------------
void cdvdman_read_set_stm0_callback(StmCallback_t callback)
{
    Stm0Callback = callback;
}

//-------------------------------------------------------------------------
// Exported API function
u32 sceCdGetReadPos(void)
{
    M_DEBUG("%s() = %d\n", __FUNCTION__, ReadPos);

    return ReadPos;
}
