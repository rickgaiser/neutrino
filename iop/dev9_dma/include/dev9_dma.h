#ifndef DEV9_DMA_H
#define DEV9_DMA_H


#include <types.h>
#include <irx.h>


struct dev9_dma_transfer;
typedef void (*dev9_dma_cb)(struct dev9_dma_transfer *tr, void *arg);


struct dev9_dma_client {
	u32 channel; /* 0,1,2 or 3 */
 	u32 dmactrl;
 	u32 block_shift;
 	u32 block_size4;

	dev9_dma_cb cb_pre_dma;
	dev9_dma_cb cb_post_dma;
	dev9_dma_cb cb_done;
};

struct dev9_dma_transfer {
	struct dev9_dma_client *cl;

	u32 maddr;
	u32 bcr;
	u32 chcr;

	void *cb_arg;
};

#define dev9_dma_IMPORTS_start DECLARE_IMPORT_TABLE(dev9_dma, 1, 0)
#define dev9_dma_IMPORTS_end END_IMPORT_TABLE

int  dev9_dma_transfer(struct dev9_dma_client *cl, void *addr, u32 size, u32 dir, void *cb_arg);
#define I_dev9_dma_transfer DECLARE_IMPORT(4, dev9_dma_transfer)

int  dev9_dma_client_init(struct dev9_dma_client *cl, u32 channel, u32 block_shift);
#define I_dev9_dma_client_init DECLARE_IMPORT(5, dev9_dma_client_init)


#endif
