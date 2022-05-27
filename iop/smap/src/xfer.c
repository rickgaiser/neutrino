#include <errno.h>
#include <stdio.h>
#include <dmacman.h>
#include <dev9.h>
#include <intrman.h>
#include <loadcore.h>
#include <modload.h>
#include <stdio.h>
#include <sysclib.h>
#include <thbase.h>
#include <thevent.h>
#include <thsemap.h>
#include <irx.h>

#include <smapregs.h>
#include <speedregs.h>

#include "main.h"

#include "xfer.h"
#include "udpbd.h"

extern struct SmapDriverData SmapDriverData;
static int tx_sema = -1;
static int tx_done_ev = -1;


static void Dev9PreDmaCbHandler(int bcr, int dir)
{
    volatile u8 *smap_regbase;
    u16 SliceCount;

    smap_regbase = SmapDriverData.smap_regbase;
    SliceCount = bcr >> 16;
    if (dir != DMAC_TO_MEM) {
        SMAP_REG16(SMAP_R_TXFIFO_SIZE) = SliceCount;
        SMAP_REG8(SMAP_R_TXFIFO_CTRL) = SMAP_TXFIFO_DMAEN;
    } else {
        SMAP_REG16(SMAP_R_RXFIFO_SIZE) = SliceCount;
        SMAP_REG8(SMAP_R_RXFIFO_CTRL) = SMAP_RXFIFO_DMAEN;
    }
}

static void Dev9PostDmaCbHandler(int bcr, int dir)
{
    volatile u8 *smap_regbase;

    smap_regbase = SmapDriverData.smap_regbase;
    if (dir != DMAC_TO_MEM) {
        while (SMAP_REG8(SMAP_R_TXFIFO_CTRL) & SMAP_TXFIFO_DMAEN) {};
    } else {
        while (SMAP_REG8(SMAP_R_RXFIFO_CTRL) & SMAP_RXFIFO_DMAEN) {};
    }
}

void xfer_init(void)
{
    iop_sema_t sema_info;
    iop_event_t event_info;

    sema_info.attr    = 0;
    sema_info.initial = 1; /* Unlocked.  */
    sema_info.max     = 1;
    tx_sema = CreateSema(&sema_info);

    event_info.attr   = 0;
    event_info.option = 0;
    event_info.bits   = 0;
    tx_done_ev = CreateEventFlag(&event_info);

    dev9RegisterPreDmaCb(1, &Dev9PreDmaCbHandler);
    dev9RegisterPostDmaCb(1, &Dev9PostDmaCbHandler);
}

static int SmapDmaTransfer(volatile u8 *smap_regbase, void *buffer, unsigned int size, int direction)
{
    unsigned int NumBlocks;
    int result;

    /*  Non-Sony: the original block size was (32*4 = 128) bytes.
        However, that resulted in slightly lower performance due to the IOP needing to copy more data.    */
    if ((NumBlocks = size >> 6) > 0) {
        if (dev9DmaTransfer(1, buffer, NumBlocks << 16 | 0x10, direction) >= 0) {
            result = NumBlocks << 6;
        } else
            result = 0;
    } else
        result = 0;

    return result;
}

static inline void CopyFromFIFO(volatile u8 *smap_regbase, void *buffer, unsigned int length, u16 RxBdPtr)
{
    int i, result;

    SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = RxBdPtr;

    if ((result = SmapDmaTransfer(smap_regbase, buffer, length, DMAC_TO_MEM)) < 0) {
        result = 0;
    }

    for (i = result; i < length; i += 4) {
        ((u32 *)buffer)[i / 4] = SMAP_REG32(SMAP_R_RXFIFO_DATA);
    }
}

static inline void CopyToFIFO(volatile u8 *smap_regbase, const void *buffer, unsigned int length)
{
    int i, result;

    if ((result = SmapDmaTransfer(smap_regbase, (void *)buffer, length, DMAC_FROM_MEM)) < 0) {
        result = 0;
    }

    for (i = result; i < length; i += 4) {
        SMAP_REG32(SMAP_R_TXFIFO_DATA) = ((u32 *)buffer)[i / 4];
    }
}

int HandleRxIntr(struct SmapDriverData *SmapDrivPrivData)
{
    USE_SMAP_RX_BD;
    int NumPacketsReceived;
    volatile smap_bd_t *PktBdPtr;
    volatile u8 *smap_regbase;
    //void *pbuf, *payload;
    u16 ctrl_stat, length, pointer, LengthRounded;

    smap_regbase = SmapDrivPrivData->smap_regbase;

    NumPacketsReceived = 0;

    /*  Non-Sony: Workaround for the hardware BUG whereby the Rx FIFO of the MAL becomes unresponsive or loses frames when under load.
        Check that there are frames to process, before accessing the BD registers. */
    while (SMAP_REG8(SMAP_R_RXFIFO_FRAME_CNT) > 0) {
        PktBdPtr = &rx_bd[SmapDrivPrivData->RxBDIndex % SMAP_BD_MAX_ENTRY];
        ctrl_stat = PktBdPtr->ctrl_stat;
        if (!(ctrl_stat & SMAP_BD_RX_EMPTY)) {
            length = PktBdPtr->length;
            LengthRounded = (length + 3) & ~3;
            pointer = PktBdPtr->pointer;

            if (ctrl_stat & (SMAP_BD_RX_INRANGE | SMAP_BD_RX_OUTRANGE | SMAP_BD_RX_FRMTOOLONG | SMAP_BD_RX_BADFCS | SMAP_BD_RX_ALIGNERR | SMAP_BD_RX_SHORTEVNT | SMAP_BD_RX_RUNTFRM | SMAP_BD_RX_OVERRUN)) {
                // Original did this whenever a frame is dropped.
                SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = pointer + LengthRounded;
            } else {
                if (handle_rx_eth(pointer) < 0) {
                    // Original did this whenever a frame is dropped.
                    SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = pointer + LengthRounded;
                }
            }

            SMAP_REG8(SMAP_R_RXFIFO_FRAME_DEC) = 0;
            PktBdPtr->ctrl_stat = SMAP_BD_RX_EMPTY;
            // PktBdPtr->reserved=0;
            // PktBdPtr->length=0;
            // PktBdPtr->pointer=0;
            SmapDrivPrivData->RxBDIndex++;
        } else
            break;
    }

    return NumPacketsReceived;
}

static void *g_buf = NULL;
static size_t g_bufsize = 0;
int smap_transmit(void *buf, size_t size)
{
    int rv = -1;
    //u32 EFBits;

    WaitSema(tx_sema);
    if (g_buf == NULL) {
        // Queue transmission
        g_buf = buf;
        g_bufsize = size;
        SetEventFlag(SmapDriverData.Dev9IntrEventFlag, SMAP_EVENT_XMIT);
        // Wait for transmission to dequed
        //WaitEventFlag(tx_done_ev, 1, WEF_OR | WEF_CLEAR, &EFBits);
        rv = 0;
    }
    SignalSema(tx_sema);

    return rv;
}

static int smap_tx_get(void **data)
{
    int rv = 0;

    if (g_buf != NULL) {
        // Dequeue
        *data = g_buf;
        rv = g_bufsize;
        g_buf = NULL;
        g_bufsize = 0;
        //SetEventFlag(tx_done_ev, 1);
    }

    return rv;
}

int HandleTxReqs(struct SmapDriverData *SmapDrivPrivData)
{
    int result, length;
    void *data;
    USE_SMAP_TX_BD;
    volatile u8 *smap_regbase;
    volatile smap_bd_t *BD_ptr;
    u16 BD_data_ptr;
    unsigned int SizeRounded;

    result = 0;
    while (1) {
        if ((length = smap_tx_get(&data)) < 1) {
            return result;
        }
        SmapDrivPrivData->packetToSend = data;

        if (SmapDrivPrivData->NumPacketsInTx < SMAP_BD_MAX_ENTRY) {
            if (length > 0) {
                SizeRounded = (length + 3) & ~3;

                if (SmapDrivPrivData->TxBufferSpaceAvailable >= SizeRounded) {
                    smap_regbase = SmapDrivPrivData->smap_regbase;

                    BD_data_ptr = SMAP_REG16(SMAP_R_TXFIFO_WR_PTR) + SMAP_TX_BASE;
                    BD_ptr = &tx_bd[SmapDrivPrivData->TxBDIndex % SMAP_BD_MAX_ENTRY];

                    CopyToFIFO(SmapDrivPrivData->smap_regbase, data, length);

                    result++;
                    BD_ptr->length = length;
                    BD_ptr->pointer = BD_data_ptr;
                    SMAP_REG8(SMAP_R_TXFIFO_FRAME_INC) = 0;
                    BD_ptr->ctrl_stat = SMAP_BD_TX_READY | SMAP_BD_TX_GENFCS | SMAP_BD_TX_GENPAD;
                    SmapDrivPrivData->TxBDIndex++;
                    SmapDrivPrivData->NumPacketsInTx++;
                    SmapDrivPrivData->TxBufferSpaceAvailable -= SizeRounded;
                } else
                    return result; // Out of FIFO space
            } else
                ;//printf("smap: dropped\n");
        } else
            return result; // Queue full

        SmapDrivPrivData->packetToSend = NULL;
    }
}
