#ifndef XFER_H
#define XFER_H


#include <stdint.h>
#include "main.h"


/**
 * Send data over the network
 * @param header packet header buffer
 * @param headersize Size of the header in bytes
 * @param data data buffer to be sent
 * @param datasize Size of the data in bytes
 * @return 0 on success, -1 on failure
 */
int smap_transmit(void *header, uint16_t headersize, const void *data, uint16_t datasize);

/**
 * Register a callback for received Ethernet frames.
 * Called from interrupt context (SMAP IST thread).
 * @param cb Callback: receives frame length; returns 0 on success, -1 to drop
 * @return 0 on success
 */
int smap_register_rx_callback(int (*cb)(uint16_t len));

/**
 * Read bytes from the current RX frame in the SMAP FIFO.
 * Must be called from within the RX callback registered via smap_register_rx_callback.
 * Uses DMA for large transfers (>= 64 bytes), PIO otherwise.
 * @param offset Frame-relative byte offset (0 = start of Ethernet header)
 * @param dst    Destination buffer (must be 4-byte aligned for DMA path)
 * @param bytes  Number of bytes to read (2 or multiple of 4)
 */
void smap_fifo_read(uint16_t offset, void *dst, uint32_t bytes);

void xfer_init(void);
int HandleRxIntr(struct SmapDriverData *SmapDrivPrivData);


#endif
