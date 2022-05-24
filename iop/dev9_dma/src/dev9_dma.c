#include "dev9_dma.h"

#include <loadcore.h>
#include <intrman.h>
#include <dmacman.h>
#include <dev9regs.h>
#include <speedregs.h>

#include <stdio.h>


#define MODNAME "dev9_dma"
IRX_ID(MODNAME, 1, 0);
extern struct irx_export_table _exp_dev9_dma;


#define M_PRINTF(format, args...) printf(MODNAME ": " format, ## args)
#define M_ERROR(format, args...) M_PRINTF("ERROR: " format, ## args)
#if 0
	#define M_DEBUG M_PRINTF
#else
	#define M_DEBUG(format, args...)
#endif


#define DEV9_DMA_QUEUE_SIZE 32
static struct dev9_dma_transfer tqueue[DEV9_DMA_QUEUE_SIZE];
static int tqueue_write_idx = 0;
static int tqueue_read_idx = 0;
#define QUEUE_NEXT(idx)	((idx + 1) & (DEV9_DMA_QUEUE_SIZE-1))
#define QUEUE_EMPTY()	(tqueue_write_idx == tqueue_read_idx)
#define QUEUE_FULL()	(QUEUE_NEXT(tqueue_write_idx) == tqueue_read_idx)


static void _dev9_dma_complete(struct dev9_dma_transfer *tr);


/*
 * private functions
 */
static void
_dev9_dma_start(struct dev9_dma_transfer *tr)
{
	USE_SPD_REGS;
	volatile iop_dmac_chan_t *dev9_chan = (iop_dmac_chan_t *)DEV9_DMAC_BASE;
	struct dev9_dma_client *cl = tr->cl;

	//M_DEBUG("%s\n", __func__);

	SPD_REG16(SPD_R_DMA_CTRL) = cl->dmactrl;

	/* Call pre-dma callback handler */
	if (cl->cb_pre_dma != NULL)
		cl->cb_pre_dma(tr, tr->cb_arg);

	dev9_chan->madr = tr->maddr;
	dev9_chan->bcr  = tr->bcr;
	dev9_chan->chcr = tr->chcr;
}

static void
_dev9_dma_try_start(struct dev9_dma_transfer *tr)
{
	//M_DEBUG("%s\n", __func__);

	if ((tr->bcr >> 16) <= 0)
		_dev9_dma_complete(tr);
	else
		_dev9_dma_start(tr);
}

static void
_dev9_dma_complete(struct dev9_dma_transfer *tr)
{
	struct dev9_dma_client *cl = tr->cl;

	//M_DEBUG("%s\n", __func__);

	/* Call post-dma callback handler */
	if (cl->cb_post_dma != NULL)
		cl->cb_post_dma(tr, tr->cb_arg);

	/* Call DMA done callback handler */
	/* NOTE: This callback can place another transfer in the queue */
	if (cl->cb_done != NULL)
		cl->cb_done(tr, tr->cb_arg);

	/* Remove transfer from queue */
	tqueue_read_idx = QUEUE_NEXT(tqueue_read_idx);

	/* Start next DMA transfer if queue not empty */
	if (!QUEUE_EMPTY())
		_dev9_dma_try_start(&tqueue[tqueue_read_idx]);

	/* Invalidate the transfer */
	tr->maddr = 0;
}

static int
_dev9_dma_intr_handler(void *arg)
{
	struct dev9_dma_transfer *tr = &tqueue[tqueue_read_idx];

	//M_DEBUG("%s\n", __func__);

	/* Validate the transfer */
	if (tr->maddr == 0) {
		M_ERROR("%s: spurious interrupt\n", __func__);
		return 1;
	}

	_dev9_dma_complete(tr);

	return 1;
}

/*
 * public functions
 */
int
dev9_dma_transfer(struct dev9_dma_client *cl, void *addr, u32 size, u32 dir, void *cb_arg)
{
	struct dev9_dma_transfer *tr;
	int flags, dma_stopped;

	M_DEBUG("%s: size=%lu, dir=%s\n", __func__, size, (dir == DMAC_FROM_MEM) ? "write" : "read");

	CpuSuspendIntr(&flags);

	/* Get unused transfer from queue */
	if (QUEUE_FULL()) {
		CpuResumeIntr(flags);
		return -1;
	}
	dma_stopped = QUEUE_EMPTY() ? 1 : 0;
	tr = &tqueue[tqueue_write_idx];
	tqueue_write_idx = QUEUE_NEXT(tqueue_write_idx);

	tr->cl      = cl;
 	tr->maddr   = (u32)addr;
	tr->bcr     = (size >> cl->block_shift) << 16 | cl->block_size4;
	tr->chcr    = DMAC_CHCR_30|DMAC_CHCR_TR|DMAC_CHCR_CO|(dir & DMAC_CHCR_DR);
	tr->cb_arg  = cb_arg;

	/* Start the DMA engine if it was stopped */
	if (dma_stopped == 1)
		_dev9_dma_try_start(tr);

	CpuResumeIntr(flags);

	return 0;
}

int
dev9_dma_client_init(struct dev9_dma_client *cl, u32 channel, u32 block_shift)
{
	USE_SPD_REGS;
	u32 dmactrl;

	M_DEBUG("%s\n", __func__);

	switch(channel){
	case 0:
	case 1:	dmactrl = channel;
		break;
	case 2:
	case 3:	dmactrl = (4 << channel);
		break;
	default:
		return -1;
	}

	cl->channel = channel;
	cl->dmactrl = (SPD_REG16(SPD_R_REV_1) < 17) ? (dmactrl & 0x03) | 0x04 : (dmactrl & 0x01) | 0x06;
	cl->block_shift = block_shift;
	cl->block_size4 = (1 << block_shift) / 4;
	cl->cb_pre_dma = NULL;
	cl->cb_post_dma = NULL;
	cl->cb_done = NULL;

	return 0;
}

int
_start(int argc, char *argv[])
{
	int i;

	M_DEBUG("%s\n", __func__);

	for (i = 0; i < DEV9_DMA_QUEUE_SIZE; i++) {
		tqueue[i].maddr = 0;
	}

	if (RegisterLibraryEntries(&_exp_dev9_dma) != 0) {
		M_ERROR("RegisterLibraryEntries\n");
		return MODULE_NO_RESIDENT_END;
	}

	/* IOP<->DEV9 DMA completion interrupt */
	RegisterIntrHandler(IOP_IRQ_DMA_DEV9, 1, _dev9_dma_intr_handler, NULL);
	EnableIntr(IOP_IRQ_DMA_DEV9);

	M_PRINTF("running\n");

	return MODULE_RESIDENT_END;
}
