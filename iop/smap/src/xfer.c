#include <dmacman.h>
#include <dev9.h>
#include <thevent.h>
#include <thsemap.h>
#include <smapregs.h>

#include "xfer.h"
#include "main.h"

extern struct SmapDriverData SmapDriverData;
extern u16 g_dma_max_block_size;

static int tx_sema = -1;
static int tx_done_ev = -1;

/* RX callback state */
static u16 g_current_frame_ptr = 0;
static uint8_t g_hdr_buf[64] __attribute__((aligned(4)));
static uint16_t g_hdr_read_bytes = 0;
static int (*g_rx_callback)(uint16_t len, const uint8_t *hdr, uint16_t hdr_len) = NULL;


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

int smap_register_rx_callback(int (*cb)(uint16_t len, const uint8_t *hdr, uint16_t hdr_len), uint16_t n_bytes)
{
    g_rx_callback = cb;
    g_hdr_read_bytes = (n_bytes > sizeof(g_hdr_buf)) ? sizeof(g_hdr_buf) : n_bytes;
    return 0;
}

/*
 * Read bytes from the current RX frame (frame-relative offset).
 * Uses DMA for transfers >= 64 bytes, PIO otherwise.
 * Seeks to (g_current_frame_ptr + offset) before reading.
 */
void smap_fifo_read(uint16_t offset, void *dst, uint32_t bytes)
{
    volatile u8 *smap_regbase = SmapDriverData.smap_regbase;
    u32 dma_bytes = 0;
    u32 bcr;
    u8 *d = (u8 *)dst;
    u32 i;

    SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = g_current_frame_ptr + offset;

    if (bytes >= 64) {
        /* DMA path: absorbs _fifo_to_mem logic from udprdma.c */
        /* Try exact-fit with large blocks (most efficient, no PIO needed) */
        if (bytes >= 512 && g_dma_max_block_size >= 512 && (bytes & 511) == 0) {
            dma_bytes = bytes;
            bcr = (bytes >> 9) << 16 | 128;   /* 512-byte = 128-word blocks */
        } else if (bytes >= 256 && g_dma_max_block_size >= 256 && (bytes & 255) == 0) {
            dma_bytes = bytes;
            bcr = (bytes >> 8) << 16 | 64;    /* 256-byte = 64-word blocks */
        } else if (bytes >= 128 && (bytes & 127) == 0) {
            dma_bytes = bytes;
            bcr = (bytes >> 7) << 16 | 32;    /* 128-byte = 32-word blocks */
        }
        /* Partial fit: DMA floor portion with 128-byte blocks, PIO the rest */
        else if (bytes >= 128) {
            dma_bytes = bytes & ~127;
            bcr = (dma_bytes >> 7) << 16 | 32;
        }
        /* Small data: 64-byte blocks + PIO */
        else {
            dma_bytes = bytes & ~63;
            bcr = (dma_bytes >> 6) << 16 | 16; /* 64-byte = 16-word blocks */
        }

        if (dma_bytes > 0)
            dev9DmaTransfer(1, dst, bcr, DMAC_TO_MEM);
    }

    /* PIO remaining bytes (u32 chunks, then u16 for trailing 2 bytes) */
    for (i = dma_bytes; i + 4 <= bytes; i += 4)
        *(u32 *)(d + i) = SMAP_REG32(SMAP_R_RXFIFO_DATA);
    if (i + 2 <= bytes)
        *(u16 *)(d + i) = SMAP_REG16(SMAP_R_RXFIFO_DATA);
}

int HandleRxIntr(struct SmapDriverData *SmapDrivPrivData)
{
    USE_SMAP_RX_BD;
    int NumPacketsReceived;
    volatile smap_bd_t *PktBdPtr;
    volatile u8 *smap_regbase;
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
                SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = pointer + LengthRounded;
            } else {
                g_current_frame_ptr = pointer;
                if (g_hdr_read_bytes > 0) {
                    uint16_t hdr_len = (g_hdr_read_bytes < length) ? g_hdr_read_bytes : length;
                    smap_fifo_read(0, g_hdr_buf, hdr_len);
                    if (g_rx_callback == NULL || g_rx_callback(length, g_hdr_buf, hdr_len) < 0)
                        SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = pointer + LengthRounded;
                } else {
                    if (g_rx_callback == NULL || g_rx_callback(length, NULL, 0) < 0)
                        SMAP_REG16(SMAP_R_RXFIFO_RD_PTR) = pointer + LengthRounded;
                }
            }

            SMAP_REG8(SMAP_R_RXFIFO_FRAME_DEC) = 0;
            PktBdPtr->ctrl_stat = SMAP_BD_RX_EMPTY;
            SmapDrivPrivData->RxBDIndex++;
        } else
            break;
    }

    return NumPacketsReceived;
}

static int HandleTxReqs(struct SmapDriverData *SmapDrivPrivData, void *header, uint16_t headersize, const void *data, uint16_t datasize)
{
    USE_SMAP_EMAC3_REGS;
    USE_SMAP_TX_BD;
    volatile u8 *smap_regbase;
    volatile smap_bd_t *BD_ptr;
    u16 BD_data_ptr;
    unsigned int SizeRounded = (headersize + datasize + 3) & ~3;

    while (SmapDrivPrivData->NumPacketsInTx > 0) {
        u16 ctrl_stat = tx_bd[SmapDrivPrivData->TxDNVBDIndex % SMAP_BD_MAX_ENTRY].ctrl_stat;
        if (ctrl_stat & SMAP_BD_TX_READY)
            break;
        SmapDrivPrivData->TxBufferSpaceAvailable += (tx_bd[SmapDrivPrivData->TxDNVBDIndex & (SMAP_BD_MAX_ENTRY - 1)].length + 3) & ~3;
        SmapDrivPrivData->TxDNVBDIndex++;
        SmapDrivPrivData->NumPacketsInTx--;
    }

    if (SmapDrivPrivData->NumPacketsInTx >= SMAP_BD_MAX_ENTRY)
        return -1;

    if (SmapDrivPrivData->TxBufferSpaceAvailable < SizeRounded)
        return -2;

    smap_regbase = SmapDrivPrivData->smap_regbase;

    BD_data_ptr = SMAP_REG16(SMAP_R_TXFIFO_WR_PTR) + SMAP_TX_BASE;
    BD_ptr = &tx_bd[SmapDrivPrivData->TxBDIndex % SMAP_BD_MAX_ENTRY];

    if (headersize > 0)
        CopyToFIFO(SmapDrivPrivData->smap_regbase, header, headersize);
    if (datasize > 0)
        CopyToFIFO(SmapDrivPrivData->smap_regbase, data, datasize);

    BD_ptr->length = headersize + datasize;
    BD_ptr->pointer = BD_data_ptr;
    SMAP_REG8(SMAP_R_TXFIFO_FRAME_INC) = 0;
    BD_ptr->ctrl_stat = SMAP_BD_TX_READY | SMAP_BD_TX_GENFCS | SMAP_BD_TX_GENPAD;
    SmapDrivPrivData->TxBDIndex++;
    SmapDrivPrivData->NumPacketsInTx++;
    SmapDrivPrivData->TxBufferSpaceAvailable -= SizeRounded;

    SMAP_EMAC3_SET32(SMAP_R_EMAC3_TxMODE0, SMAP_E3_TX_GNP_0);

    return 1;
}

int smap_transmit(void *header, uint16_t headersize, const void *data, uint16_t datasize)
{
    WaitSema(tx_sema);

    // Add packet to queue (if there's room)
    while (HandleTxReqs(&SmapDriverData, header, headersize, data, datasize) < 0) {
        // Wait for about 1KiB (at a speed of 100Mbps)
        // FIXME! We want a blocking write, this works but it's not ideal.
        DelayThread(100);
    }

    SignalSema(tx_sema);

    return 0;
}
