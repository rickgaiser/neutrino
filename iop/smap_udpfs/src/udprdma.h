#ifndef UDPRDMA_H
#define UDPRDMA_H

/*
 * UDPRDMA - Reliable RDMA over UDP for PS2
 *
 * Public API for reliable data transfer over UDP.
 * Provides service discovery and Go-Back-N ARQ reliability.
 */

#include <stdint.h>
#include "udprdma_packet.h"


/* Error codes */
#define UDPRDMA_OK            0
#define UDPRDMA_ERR_TIMEOUT  -1  /* Operation timed out */
#define UDPRDMA_ERR_NACK     -2  /* Too many retransmits */
#define UDPRDMA_ERR_NOSOCKET -3  /* No socket available */
#define UDPRDMA_ERR_INVAL    -4  /* Invalid argument */
#define UDPRDMA_ERR_NOTCONN  -5  /* Not connected */
#define UDPRDMA_ERR_NOBUF    -6  /* No buffer available */

/* Opaque socket handle */
typedef struct udprdma_socket udprdma_socket_t;


/**
 * Create UDPRDMA socket
 *
 * @param port       UDP port to bind (0 for default UDPFS_PORT)
 * @param service_id Service identifier for discovery
 * @return Socket handle, or NULL on error
 */
udprdma_socket_t *udprdma_create(uint16_t port, uint16_t service_id);

/**
 * Destroy UDPRDMA socket
 *
 * @param socket Socket handle
 */
void udprdma_destroy(udprdma_socket_t *socket);

/**
 * Discover peer (client mode)
 *
 * Broadcasts DISCOVERY messages and waits for INFORM response.
 * Blocks until peer is found or timeout.
 *
 * @param socket     Socket handle
 * @param timeout_ms Timeout in milliseconds (0 for default 2000ms)
 * @return UDPRDMA_OK on success, error code on failure
 */
int udprdma_discover(udprdma_socket_t *socket, uint32_t timeout_ms);

/**
 * Send INFORM message (server mode)
 *
 * Announces service availability. Call this periodically or
 * after receiving a DISCOVERY message.
 *
 * @param socket Socket handle
 */
void udprdma_inform(udprdma_socket_t *socket);

/**
 * Send data reliably
 *
 * Sends data and waits for ACK. Retransmits on NACK or timeout.
 * Blocks until acknowledged or max retries exceeded.
 *
 * @param socket Socket handle
 * @param data   Data to send (must be 4-byte aligned)
 * @param size   Size in bytes (will be rounded up to 4-byte boundary)
 * @return UDPRDMA_OK on success, error code on failure
 */
int udprdma_send(udprdma_socket_t *socket, const void *data, uint32_t size);

/**
 * Send data reliably with scatter-gather (zero-copy)
 *
 * Packs app_hdr into the packet header and sends data directly from
 * the caller's buffer via DMA. Avoids memcpy for large payloads.
 *
 * @param socket       Socket handle
 * @param app_hdr      App-level header (max UDPRDMA_MAX_APP_HDR bytes, must be 4-byte aligned size)
 * @param app_hdr_size Size of app_hdr in bytes (must be multiple of 4)
 * @param data         Data to send directly via DMA (must be 4-byte aligned)
 * @param data_size    Size of data in bytes
 * @return UDPRDMA_OK on success, error code on failure
 */
int udprdma_send_ll(udprdma_socket_t *socket,
                    const void *app_hdr, uint32_t app_hdr_size,
                    const void *data, uint32_t data_size);

/**
 * Receive data
 *
 * Waits for incoming data packet. Automatically sends ACK.
 * Blocks until data arrives or timeout.
 *
 * @param socket     Socket handle
 * @param buffer     Buffer to receive data (must be 4-byte aligned)
 * @param size       Buffer size in bytes
 * @param timeout_ms Timeout in milliseconds (0 for default 500ms)
 * @return Number of bytes received, or negative error code
 */
int udprdma_recv(udprdma_socket_t *socket, void *buffer, uint32_t size, uint32_t timeout_ms);

/**
 * Check if connected to peer
 *
 * @param socket Socket handle
 * @return 1 if connected, 0 if not
 */
int udprdma_is_connected(udprdma_socket_t *socket);

/**
 * Get peer IP address
 *
 * @param socket Socket handle
 * @return Peer IP address, or 0 if not connected
 */
uint32_t udprdma_get_peer_ip(udprdma_socket_t *socket);

/**
 * Set receive buffer
 *
 * Sets the buffer used for receiving data via DMA.
 * Must be called before udprdma_recv() if using DMA.
 *
 * @param socket Socket handle
 * @param buffer Buffer for receiving data (must be 4-byte aligned)
 * @param size   Buffer size in bytes
 */
void udprdma_set_rx_buffer(udprdma_socket_t *socket, void *buffer, uint32_t size);

/**
 * Set receive app header buffer
 *
 * When set, the IST extracts the app header (hdr_word_count*4 bytes) via PIO
 * into this buffer, while data goes to the rx_buffer via DMA.
 * Reset automatically after udprdma_recv() completes.
 *
 * @param socket   Socket handle
 * @param hdr_buf  Buffer for app header (must be 4-byte aligned), or NULL to disable
 * @param hdr_size Expected header size in bytes (must be multiple of 4)
 */
void udprdma_set_rx_app_header(udprdma_socket_t *socket, void *hdr_buf, uint32_t hdr_size);


#endif /* UDPRDMA_H */
