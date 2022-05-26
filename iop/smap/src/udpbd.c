#include <errno.h>
#include <bdm.h>
#include <thevent.h>
#include <stdio.h>
#include <smapregs.h>
#include <dmacman.h>
#include <dev9.h>

#include "udpbd.h"
#include "main.h"

#define M_PRINTF(format, args...) printf("UDPBD: " format, ##args)
#define M_DEBUG                   M_PRINTF
//#define M_DEBUG(...)

#define UDPBD_MAX_RETRIES         4
static struct block_device g_udpbd;
static udpbd_pkt_t g_pkt;
static u8 g_cmdid        = 0;
static int g_read_done   = 0;
static int ev_worker     = 0;
static int g_read_cmdpkt = 0;
static int bdm_connected = 0;
static void *g_buffer;
static unsigned int g_read_size;
static unsigned int g_errno = 0;


static unsigned int _udpbd_read_timeout(void *arg)
{
    g_read_size = 0;
    g_errno     = 1;
    iSetEventFlag(g_read_done, 2);
    return 0;
}

extern struct SmapDriverData SmapDriverData;
static void debug_smap()
{
    USE_SMAP_EMAC3_REGS;
    USE_SMAP_REGS;
    USE_SMAP_RX_BD;
    int i;
    char bdidx;

    M_DEBUG("SMAP_R_RXFIFO_CTRL:       0x%x\n", SMAP_REG8(SMAP_R_RXFIFO_CTRL));
    DelayThread(1000);
    M_DEBUG("SMAP_R_RXFIFO_RD_PTR:     0x%x\n", SMAP_REG16(SMAP_R_RXFIFO_RD_PTR));
    DelayThread(1000);
    //M_DEBUG("SMAP_R_RXFIFO_SIZE:       %d\n", SMAP_REG16(SMAP_R_RXFIFO_SIZE));
    M_DEBUG("SMAP_R_RXFIFO_FRAME_CNT:  %d\n", SMAP_REG8(SMAP_R_RXFIFO_FRAME_CNT));
    DelayThread(1000);
    //M_DEBUG("SMAP_R_RXFIFO_FRAME_DEC:  %d\n", SMAP_REG8(SMAP_R_RXFIFO_FRAME_DEC));
    M_DEBUG("SMAP_R_EMAC3_RxMODE:      0x%x\n", (unsigned int)SMAP_EMAC3_GET32(SMAP_R_EMAC3_RxMODE));
    DelayThread(1000);
    M_DEBUG("SMAP_R_EMAC3_INTR_STAT:   0x%x\n", (unsigned int)SMAP_EMAC3_GET32(SMAP_R_EMAC3_INTR_STAT));
    DelayThread(1000);
    M_DEBUG("SMAP_R_EMAC3_INTR_ENABLE: 0x%x\n", (unsigned int)SMAP_EMAC3_GET32(SMAP_R_EMAC3_INTR_ENABLE));
    DelayThread(1000);

    bdidx = SmapDriverData.RxBDIndex % SMAP_BD_MAX_ENTRY;
    for (i = 0; i < SMAP_BD_MAX_ENTRY; i++)
    {
        if (rx_bd[i].ctrl_stat != SMAP_BD_RX_EMPTY ||
            rx_bd[i].reserved != 0 ||
            rx_bd[i].length != 0 ||
            rx_bd[i].pointer != 0 ||
            i == bdidx)
        {
            if (i == bdidx)
                M_DEBUG(" - rx_bd[%d]: 0x%x / 0x%x / %d / 0x%x <--\n", i, rx_bd[i].ctrl_stat, rx_bd[i].reserved, rx_bd[i].length, rx_bd[i].pointer);
            else
                M_DEBUG(" - rx_bd[%d]: 0x%x / 0x%x / %d / 0x%x\n", i, rx_bd[i].ctrl_stat, rx_bd[i].reserved, rx_bd[i].length, rx_bd[i].pointer);
            DelayThread(1000);
        }
    }

    M_DEBUG("RxDroppedFrameCount:      %d\n", (int)SmapDriverData.RuntimeStats.RxDroppedFrameCount);
    DelayThread(1000);
    M_DEBUG("RxErrorCount:             %d\n", (int)SmapDriverData.RuntimeStats.RxErrorCount);
    DelayThread(1000);
    M_DEBUG("RxFrameOverrunCount:      %d\n", SmapDriverData.RuntimeStats.RxFrameOverrunCount);
    DelayThread(1000);
    M_DEBUG("RxFrameBadLengthCount:    %d\n", SmapDriverData.RuntimeStats.RxFrameBadLengthCount);
    DelayThread(1000);
    M_DEBUG("RxFrameBadFCSCount:       %d\n", SmapDriverData.RuntimeStats.RxFrameBadFCSCount);
    DelayThread(1000);
    M_DEBUG("RxFrameBadAlignmentCount: %d\n", SmapDriverData.RuntimeStats.RxFrameBadAlignmentCount);
    DelayThread(1000);
    
    //M_DEBUG("TxDroppedFrameCount:      %d\n", (int)SmapDriverData.RuntimeStats.TxDroppedFrameCount);
    //DelayThread(1000);
    //M_DEBUG("TxErrorCount:             %d\n", (int)SmapDriverData.RuntimeStats.TxErrorCount);
    //DelayThread(1000);
    //M_DEBUG("TxFrameLOSSCRCount:       %d\n", SmapDriverData.RuntimeStats.TxFrameLOSSCRCount);
    //DelayThread(1000);
    //M_DEBUG("TxFrameEDEFERCount:       %d\n", SmapDriverData.RuntimeStats.TxFrameEDEFERCount);
    //DelayThread(1000);
    //M_DEBUG("TxFrameCollisionCount:    %d\n", SmapDriverData.RuntimeStats.TxFrameCollisionCount);
    //DelayThread(1000);
    //M_DEBUG("TxFrameUnderrunCount:     %d\n", SmapDriverData.RuntimeStats.TxFrameUnderrunCount);
    //DelayThread(1000);
    
    M_DEBUG("RxAllocFail:              %d\n", SmapDriverData.RuntimeStats.RxAllocFail);
    DelayThread(1000);
}

//
// Block device interface
//
static int _udpbd_read(struct block_device *bd, u32 sector, void *buffer, u16 count)
{
    u32 EFBits;
    iop_sys_clock_t clock;

    //M_DEBUG("%s: sector=%d, count=%d\n", __func__, sector, count);

    g_cmdid++;
    g_buffer        = buffer;
    g_read_size     = count * g_udpbd.sectorSize;
    g_read_cmdpkt   = 1; // First reply packet should be cmdpkt==1

    g_pkt.bd.magic  = UDPBD_HEADER_MAGIC;
    g_pkt.bd.cmd    = UDPBD_CMD_READ;
    g_pkt.bd.cmdid  = g_cmdid;
    g_pkt.bd.cmdpkt = 0;
    g_pkt.bd.count  = count;
    g_pkt.bd.par1   = sector;
    g_pkt.bd.par2   = 0;

    if (udp_packet_send((udp_packet_t *)&g_pkt, sizeof(udpbd_header_t) + 2) < 0)
        return -1;

    // Set alarm in case something hangs
    // 200ms + 2ms / sector
    USec2SysClock((200 * 1000) + (count * 2000), &clock);
    SetAlarm(&clock, _udpbd_read_timeout, NULL);

    //wait for data...
    WaitEventFlag(g_read_done, 2 | 1, WEF_OR | WEF_CLEAR, &EFBits);

    // Cancel alarm
    CancelAlarm(_udpbd_read_timeout, NULL);

    g_buffer      = NULL;
    g_read_size   = 0;
    g_read_cmdpkt = 0;

    switch (g_errno)
    {
        case 0:
            //M_DEBUG("%s(%d, %d): ok\n", __func__);
            break;
        case 1:
            M_DEBUG("%s(%d, %d): ERROR: timeout\n", __func__, sector, count);
            break;
        case 2:
            M_DEBUG("%s(%d, %d): ERROR: invalid packet order!\n", __func__, sector, count);
            break;
        case 3:
            M_DEBUG("%s(%d, %d): ERROR: invalid packet size!\n", __func__, sector, count);
            break;
        default:
            M_DEBUG("%s(%d, %d): ERROR: unknown %d\n", __func__, sector, count, g_errno);
            break;
    }

    if (EFBits & 1)
    { // done
        return count;
    }

    g_errno = 0;
    /*
    debug_smap();

    USE_SMAP_EMAC3_REGS;
    USE_SMAP_REGS;

    SMAP_REG16(SMAP_R_INTR_CLR) = SMAP_INTR_EMAC3;
    SMAP_EMAC3_SET32(SMAP_R_EMAC3_INTR_STAT, SMAP_E3_INTR_TX_ERR_0 | SMAP_E3_INTR_SQE_ERR_0 | SMAP_E3_INTR_DEAD_0);
    SMAP_REG16(SMAP_R_INTR_CLR) = SMAP_INTR_RXEND;
    SMAP_REG16(SMAP_R_INTR_CLR) = SMAP_INTR_RXDNV;
    SMAP_REG16(SMAP_R_INTR_CLR) = SMAP_INTR_TXDNV;

    SMAP_EMAC3_SET32(SMAP_R_EMAC3_MODE0, SMAP_E3_TXMAC_ENABLE);
    SMAP_EMAC3_SET32(SMAP_R_EMAC3_MODE0, SMAP_E3_TXMAC_ENABLE | SMAP_E3_RXMAC_ENABLE);

    SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = 0;

    DelayThread(10000);
*/
    return -EIO;
}

static int udpbd_read(struct block_device *bd, u32 sector, void *buffer, u16 count)
{
    int retries;
    u16 count_left = count;

    //M_DEBUG("%s: sector=%d, count=%d\n", __func__, sector, count);

    if (bdm_connected == 0)
        return -EIO;

    while (count_left > 0)
    {
        u16 count_block = count_left > UDPBD_MAX_SECTOR_READ ? UDPBD_MAX_SECTOR_READ : count_left;

        for (retries = 0; retries < UDPBD_MAX_RETRIES; retries++)
        {
            if (_udpbd_read(bd, sector, buffer, count_block) == count_block)
                break;
            debug_smap();
            DelayThread(10000);
        }

        if (retries == UDPBD_MAX_RETRIES)
        {
            M_DEBUG("%s: too many errors, disconnecting\n", __func__);
#ifndef NO_BDM
            bdm_disconnect_bd(&g_udpbd);
#endif
            bdm_connected = 0;
            return -EIO;
        }

        count_left -= count_block;
        sector += count_block;
        buffer += count_block * g_udpbd.sectorSize;
    }

    return count;
}

static int udpbd_write(struct block_device *bd, u32 sector, const void *buffer, u16 count)
{
    M_DEBUG("%s\n", __func__);

    return -EIO;
}

static void udpbd_flush(struct block_device *bd)
{
    M_DEBUG("%s\n", __func__);
}

static int udpbd_stop(struct block_device *bd)
{
    M_DEBUG("%s\n", __func__);

    return 0;
}

static void udpbd_worker()
{
    u32 EFBits;

    while(1) {
        WaitEventFlag(ev_worker, 1, WEF_OR | WEF_CLEAR, &EFBits);
        bdm_connect_bd(&g_udpbd);
    }
}


//
// Public functions
//
int udpbd_init(void)
{
    iop_event_t EventFlagData;
    iop_thread_t ThreadData;

    M_DEBUG("%s\n", __func__);

    EventFlagData.attr   = 0;
    EventFlagData.option = 0;
    EventFlagData.bits   = 0;
    if (g_read_done <= 0)
        g_read_done = CreateEventFlag(&EventFlagData);
    if (ev_worker <= 0)
        ev_worker = CreateEventFlag(&EventFlagData);

    ThreadData.attr = TH_C;
    ThreadData.thread = udpbd_worker;
    ThreadData.option = 0;
    ThreadData.priority = 0x20;
    ThreadData.stacksize = 0x2000;
    StartThread(CreateThread(&ThreadData), NULL);

    g_udpbd.name         = "udp";
    g_udpbd.devNr        = 0;
    g_udpbd.parNr        = 0;
    g_udpbd.sectorOffset = 0;
    g_udpbd.priv         = NULL;
    g_udpbd.read         = udpbd_read;
    g_udpbd.write        = udpbd_write;
    g_udpbd.flush        = udpbd_flush;
    g_udpbd.stop         = udpbd_stop;

    udp_packet_init((udp_packet_t *)&g_pkt, UDPBD_PORT);

    // Broadcast request for block device information
    g_pkt.bd.magic  = UDPBD_HEADER_MAGIC;
    g_pkt.bd.cmd    = UDPBD_CMD_INFO;
    g_pkt.bd.cmdid  = g_cmdid;
    g_pkt.bd.cmdpkt = 0;
    g_pkt.bd.count  = 0;
    g_pkt.bd.par1   = 0;
    g_pkt.bd.par2   = 0;
    udp_packet_send((udp_packet_t *)&g_pkt, sizeof(udpbd_header_t) + 2);

    return 0;
}

void udpbd_rx(u16 pointer)
{
    USE_SMAP_REGS;
    udpbd_header_t hdr;

    SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = pointer + 0x30;
    //hdr.magic = SMAP_REG32(SMAP_R_RXFIFO_DATA);
    hdr.cmd32 = SMAP_REG32(SMAP_R_RXFIFO_DATA);
    hdr.par1  = SMAP_REG32(SMAP_R_RXFIFO_DATA);
    hdr.par2  = SMAP_REG32(SMAP_R_RXFIFO_DATA);

    switch (hdr.cmd)
    {
        case UDPBD_CMD_INFO:
            if (bdm_connected == 0)
            {
                g_udpbd.sectorSize  = hdr.par1;
                g_udpbd.sectorCount = hdr.par2;
                bdm_connected = 1;
#ifndef NO_BDM
                SetEventFlag(ev_worker, 1);
#endif
            }
            break;
        case UDPBD_CMD_READ:
            if ((g_buffer != NULL) && (g_read_size >= hdr.par1) && (g_cmdid == hdr.cmdid))
            {
                // Validate packet order
                if (hdr.cmdpkt != (g_read_cmdpkt & 0xff))
                {
                    SmapDriverData.RuntimeStats.RxFrameOverrunCount++;
                    // Error, wakeup caller
                    g_read_size = 0;
                    g_errno     = 2;
                    SetEventFlag(g_read_done, 2);
                    break;
                }
                g_read_cmdpkt++;

                // Validate packet data size
                if ((hdr.par1 > UDPBD_MAX_DATA) || (hdr.par1 & 127))
                {
                    SmapDriverData.RuntimeStats.RxFrameBadLengthCount++;
                    // Error, wakeup caller
                    g_read_size = 0;
                    g_errno     = 3;
                    SetEventFlag(g_read_done, 2);
                    break;
                }

                // Directly DMA the packet data into the user buffer
                dev9DmaTransfer(1, (u8 *)g_buffer + ((g_read_cmdpkt - 2) * UDPBD_MAX_DATA), (hdr.par1 >> 7) << 16 | 0x20, DMAC_TO_MEM);

                g_read_size -= hdr.par1;
                if (g_read_size == 0)
                {
                    // Done, wakeup caller
                    SetEventFlag(g_read_done, 1);
                    break;
                }
            }
            else
            {
                SmapDriverData.RuntimeStats.RxFrameBadFCSCount++;
            }
            break;
        case UDPBD_CMD_WRITE:
            break;
    };
}
