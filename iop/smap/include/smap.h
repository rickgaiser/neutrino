#ifndef SMAP_H
#define SMAP_H

#include <stdint.h>
#include <irx.h>

/**
 * Read bytes from the current RX frame (frame-relative offset).
 * Must be called from within the RX callback registered via smap_register_rx_callback.
 * Uses DMA for large transfers (>= 64 bytes), PIO otherwise.
 */
void smap_fifo_read(uint16_t offset, void *dst, uint32_t bytes);

/**
 * Register a callback for received Ethernet frames.
 */
int smap_register_rx_callback(int (*cb)(uint16_t len));

/**
 * Read the hardware MAC address into a 6-byte buffer.
 */
int SMAPGetMACAddress(uint8_t *buffer);

/**
 * Transmit a packet (header + optional separate data payload).
 */
int smap_transmit(void *header, uint16_t headersize, const void *data, uint16_t datasize);

/* Import table — slot numbers match exports.tab */
#define smap_IMPORTS_start DECLARE_IMPORT_TABLE(smap, 1, 0)
#define smap_IMPORTS_end   END_IMPORT_TABLE

#define I_SMAPGetMACAddress          DECLARE_IMPORT(4, SMAPGetMACAddress)
#define I_smap_transmit              DECLARE_IMPORT(5, smap_transmit)
#define I_smap_register_rx_callback  DECLARE_IMPORT(6, smap_register_rx_callback)
#define I_smap_fifo_read             DECLARE_IMPORT(7, smap_fifo_read)

#endif
