#include <errno.h>
#include <bdm.h>
#include <thevent.h>
#include <stdio.h>
#include <smapregs.h>
#include <dmacman.h>
#include <dev9.h>

#include "udpbd.h"
#include "main.h"

#ifdef DEBUG
#define M_PRINTF(format, args...) printf("UDPBD: " format, ##args)
#define M_DEBUG(format, args...)  printf("UDPBD: " format, ##args)
#else
#define M_PRINTF(format, args...)
#define M_DEBUG(format, args...)
#endif

#define UDPBD_MAX_RETRIES         4
static struct block_device g_udpbd;
static udpbd_pkt_t g_pkt;
static uint8_t g_cmdid   = 0;
static int g_read_done   = 0;
static int g_read_cmdpkt = 0;
static int bdm_connected = 0;
static void *g_buffer;
static unsigned int g_read_size;
static unsigned int g_errno = 0;
static udp_socket_t *udpbd_socket = NULL;


static unsigned int _udpbd_read_timeout(void *arg)
{
    g_read_size = 0;
    g_errno     = 1;
    iSetEventFlag(g_read_done, 2);
    return 0;
}

//
// Block device interface
//
static int _udpbd_read(struct block_device *bd, uint32_t sector, void *buffer, uint16_t count)
{
    uint32_t EFBits;
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

    if (udp_packet_send(udpbd_socket, (udp_packet_t *)&g_pkt, sizeof(udpbd_header_t) + 2) < 0)
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
    return -EIO;
}

static int udpbd_read(struct block_device *bd, uint32_t sector, void *buffer, uint16_t count)
{
    int retries;
    uint16_t count_left = count;

    //M_DEBUG("%s: sector=%d, count=%d\n", __func__, sector, count);

    if (bdm_connected == 0)
        return -EIO;

    while (count_left > 0)
    {
        uint16_t count_block = count_left > UDPBD_MAX_SECTOR_READ ? UDPBD_MAX_SECTOR_READ : count_left;

        for (retries = 0; retries < UDPBD_MAX_RETRIES; retries++)
        {
            if (_udpbd_read(bd, sector, buffer, count_block) == count_block)
                break;
            DelayThread(1000);
        }

        if (retries == UDPBD_MAX_RETRIES)
        {
            M_DEBUG("%s: too many errors, disconnecting\n", __func__);
            bdm_disconnect_bd(&g_udpbd);
            bdm_connected = 0;
            return -EIO;
        }

        count_left -= count_block;
        sector += count_block;
        buffer += count_block * g_udpbd.sectorSize;
    }

    return count;
}

static int udpbd_write(struct block_device *bd, uint32_t sector, const void *buffer, uint16_t count)
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

static int udpbd_isr(udp_socket_t *socket, uint16_t pointer, void *arg)
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
                bdm_connect_bd(&g_udpbd);
            }
            break;
        case UDPBD_CMD_READ:
            if ((g_buffer != NULL) && (g_read_size >= hdr.par1) && (g_cmdid == hdr.cmdid))
            {
                // Validate packet order
                if (hdr.cmdpkt != (g_read_cmdpkt & 0xff))
                {
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
                    // Error, wakeup caller
                    g_read_size = 0;
                    g_errno     = 3;
                    SetEventFlag(g_read_done, 2);
                    break;
                }

                // Directly DMA the packet data into the user buffer
                dev9DmaTransfer(1, (uint8_t *)g_buffer + ((g_read_cmdpkt - 2) * UDPBD_MAX_DATA), (hdr.par1 >> 7) << 16 | 0x20, DMAC_TO_MEM);

                g_read_size -= hdr.par1;
                if (g_read_size == 0)
                {
                    // Done, wakeup caller
                    SetEventFlag(g_read_done, 1);
                    break;
                }
            }
            break;
        case UDPBD_CMD_WRITE:
            break;
    };

    return 0;
}

//
// Public functions
//
int udpbd_init(void)
{
    iop_event_t EventFlagData;

    M_DEBUG("%s\n", __func__);

    EventFlagData.attr   = 0;
    EventFlagData.option = 0;
    EventFlagData.bits   = 0;
    if (g_read_done <= 0)
        g_read_done = CreateEventFlag(&EventFlagData);

    g_udpbd.name         = "udp";
    g_udpbd.devNr        = 0;
    g_udpbd.parNr        = 0;
    g_udpbd.sectorOffset = 0;
    g_udpbd.priv         = NULL;
    g_udpbd.read         = udpbd_read;
    g_udpbd.write        = udpbd_write;
    g_udpbd.flush        = udpbd_flush;
    g_udpbd.stop         = udpbd_stop;

    // Bind to UDP socket
    udpbd_socket = udp_bind(UDPBD_PORT, udpbd_isr, NULL);

    // Broadcast request for block device information
    udp_packet_init((udp_packet_t *)&g_pkt, IP_ADDR(255,255,255,255), UDPBD_PORT);
    g_pkt.bd.magic  = UDPBD_HEADER_MAGIC;
    g_pkt.bd.cmd    = UDPBD_CMD_INFO;
    g_pkt.bd.cmdid  = g_cmdid;
    g_pkt.bd.cmdpkt = 0;
    g_pkt.bd.count  = 0;
    g_pkt.bd.par1   = 0;
    g_pkt.bd.par2   = 0;
    udp_packet_send(udpbd_socket, (udp_packet_t *)&g_pkt, sizeof(udpbd_header_t) + 2);

    return 0;
}
