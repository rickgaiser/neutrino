#ifndef XFER_H
#define XFER_H


#include "main.h"


void xfer_init(void);
int smap_transmit(void *buf, size_t size);
int HandleRxIntr(struct SmapDriverData *SmapDrivPrivData);
int HandleTxReqs(struct SmapDriverData *SmapDrivPrivData);


#endif
