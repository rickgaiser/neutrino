/*
 * UDPFS Core - File System Protocol over UDPRDMA
 *
 * Core protocol implementation for UDPFS.
 * Handles all UDPRDMA communication, message framing, retry logic, and chunking.
 */

#include <errno.h>
#include <string.h>
#include <thbase.h>
#include <stdio.h>

#include "udpfs_core.h"
#include "udpfs_packet.h"
#include "udprdma.h"
#include "main.h"


/* State */
static udprdma_socket_t *g_socket = NULL;

/* Shared buffer for write completion replies (BD and FHI) */
static uint8_t g_write_rx_buf[sizeof(udpfs_msg_write_done_t)] __attribute__((aligned(4)));

/*
 * IOMAN-specific buffers and helpers
 */
#ifdef FEATURE_UDPFS_IOMAN

/* Buffers for send/recv - must be 4-byte aligned
 * TX: max OPEN_REQ(8) + path(256) + padding = 264 bytes
 * RX: max DREAD_REPLY(48) + name(256) + padding = 304 bytes
 */
static uint8_t g_tx_buf[272] __attribute__((aligned(4)));
static uint8_t g_rx_buf[312] __attribute__((aligned(4)));

/*
 * Helper: pad a path length to 4-byte boundary
 */
static uint32_t _padded_len(uint32_t len)
{
    return (len + 3) & ~3;
}

/*
 * Helper: send request and receive reply
 */
static int _send_recv(const void *req, uint32_t req_size, void *reply, uint32_t reply_max, uint32_t timeout_ms)
{
    int ret;

    udprdma_set_rx_buffer(g_socket, reply, reply_max);

    ret = udprdma_send(g_socket, req, req_size);
    if (ret != UDPRDMA_OK) {
        M_DEBUG("udpfs: send failed: %d\n", ret);
        return ret;
    }

    ret = udprdma_recv(g_socket, reply, reply_max, timeout_ms);
    if (ret < 0) {
        M_DEBUG("udpfs: recv failed: %d\n", ret);
        return ret;
    }

    return ret;
}

/*
 * Helper: send request and receive reply
 */
static int _request(const void *req, uint32_t req_size, void *reply, uint32_t reply_max)
{
    int ret = _send_recv(req, req_size, reply, reply_max, 5000);
    if (ret < 0) {
        M_DEBUG("udpfs: request failed: %d\n", ret);
        return -EIO;
    }
    return ret;
}

#endif /* FEATURE_UDPFS_IOMAN */

/*
 * Helper: send request, receive RESULT_REPLY app header + DMA data.
 * Unified protocol for file read and block read.
 * Returns bytes received on success, 0 for EOF, negative on error.
 */
static int _recv_with_result(udprdma_socket_t *socket, const void *req, uint32_t req_size, void *buffer, uint32_t size, uint32_t timeout_ms)
{
    udpfs_msg_result_reply_t result __attribute__((aligned(4)));
    int ret;

    udprdma_set_rx_app_header(socket, &result, sizeof(result));
    udprdma_set_rx_buffer(socket, buffer, size);

    ret = udprdma_send(socket, req, req_size);
    if (ret != UDPRDMA_OK) {
        M_DEBUG("udpfs: send failed: %d\n", ret);
        return -EIO;
    }

    ret = udprdma_recv(socket, buffer, size, timeout_ms);
    if (ret < 0) {
        M_DEBUG("udpfs: recv failed: %d\n", ret);
        return -EIO;
    }

    if (result.msg_type != UDPFS_MSG_RESULT_REPLY) {
        M_DEBUG("udpfs: unexpected reply type 0x%02x\n", result.msg_type);
        return -EIO;
    }

    if (result.result <= 0)
        return result.result;  /* 0 = EOF, negative = error */

    return result.result;  /* logical bytes read (not DMA-padded count) */
}

/*
 * Helper: path-based operation for mkdir/remove/rmdir.
 */
#ifdef FEATURE_UDPFS_IOMAN
static int _path_op(uint8_t msg_type, const char *path, uint16_t mode)
{
    udpfs_msg_path_req_t *req = (udpfs_msg_path_req_t *)g_tx_buf;
    udpfs_msg_result_reply_t *reply = (udpfs_msg_result_reply_t *)g_rx_buf;
    uint32_t path_len, padded;
    int ret;

    if (!udprdma_is_connected(g_socket))
        return -EIO;

    while (path[0] == '/' || path[0] == '\\')
        path++;

    path_len = strlen(path) + 1;
    if (path_len > UDPFS_MAX_PATH)
        return -ENAMETOOLONG;
    padded = _padded_len(path_len);

    req->msg_type = msg_type;
    req->reserved = 0;
    req->mode = mode;
    memcpy(g_tx_buf + sizeof(udpfs_msg_path_req_t), path, path_len);
    memset(g_tx_buf + sizeof(udpfs_msg_path_req_t) + path_len, 0, padded - path_len);

    ret = _request(req, sizeof(udpfs_msg_path_req_t) + padded,
                   reply, sizeof(udpfs_msg_result_reply_t));
    if (ret < 0)
        return ret;

    return reply->result;
}
#endif /* FEATURE_UDPFS_IOMAN */

/*
 * Shared write helper: sends request + data chunks + waits for completion
 * req_header: WRITE_REQ (12 bytes) or BWRITE_REQ (16 bytes)
 * req_size: size of request header
 * data: buffer to write
 * total_size: total bytes to write
 */
static int _write_with_combined_header(udprdma_socket_t *socket, const void *req_header, uint32_t req_size, const void *data, uint32_t total_size)
{
    udpfs_msg_write_data_t data_hdr;
    udpfs_msg_write_done_t *done = (udpfs_msg_write_done_t *)g_write_rx_buf;
    uint8_t combined_hdr[24] __attribute__((aligned(4))); /* Max: BWRITE_REQ(16) + WRITE_DATA(8) */
    const uint8_t *buf_ptr = (const uint8_t *)data;
    uint32_t max_chunk_data = UDPFS_MAX_PAYLOAD - sizeof(udpfs_msg_write_data_t);
    uint16_t total_chunks = (total_size + max_chunk_data - 1) / max_chunk_data;
    uint32_t first_chunk_size = total_size > max_chunk_data ? max_chunk_data : total_size;
    uint32_t sent;
    uint16_t chunk_nr;
    int ret;

    /* Build first WRITE_DATA header */
    data_hdr.msg_type = UDPFS_MSG_WRITE_DATA;
    data_hdr.reserved = 0;
    data_hdr.chunk_nr = 0;
    data_hdr.chunk_size = first_chunk_size;
    data_hdr.total_chunks = total_chunks;

    /* Set up receive buffer for WRITE_DONE before sending: server may send
     * WRITE_DONE immediately after ACKing the first (and only) chunk, so
     * rx_buffer must be ready before the IST can fire. */
    udprdma_set_rx_buffer(socket, g_write_rx_buf, sizeof(g_write_rx_buf));

    /* Combine request header + WRITE_DATA header as app header, first chunk as data */
    memcpy(combined_hdr, req_header, req_size);
    memcpy(combined_hdr + req_size, &data_hdr, sizeof(data_hdr));

    ret = udprdma_send_ll(socket, combined_hdr, req_size + sizeof(data_hdr),
        buf_ptr, first_chunk_size);
    if (ret != UDPRDMA_OK) {
        M_DEBUG("udpfs: write combined send failed: %d\n", ret);
        return -EIO;
    }

    buf_ptr += first_chunk_size;
    sent = first_chunk_size;

    /* Send remaining chunks */
    for (chunk_nr = 1; sent < total_size; chunk_nr++) {
        uint32_t chunk_size = total_size - sent;
        if (chunk_size > max_chunk_data)
            chunk_size = max_chunk_data;

        data_hdr.chunk_nr = chunk_nr;
        data_hdr.chunk_size = chunk_size;

        ret = udprdma_send_ll(socket, &data_hdr, sizeof(data_hdr),
            buf_ptr, chunk_size);
        if (ret != UDPRDMA_OK) {
            M_DEBUG("udpfs: write chunk send failed: %d\n", ret);
            return -EIO;
        }

        buf_ptr += chunk_size;
        sent += chunk_size;
    }

    /* Wait for WRITE_DONE */
    ret = udprdma_recv(socket, g_write_rx_buf, sizeof(*done), 5000);
    if (ret < 0) {
        M_DEBUG("udpfs: WRITE_DONE recv failed: %d\n", ret);
        return -EIO;
    }

    if (done->msg_type != UDPFS_MSG_WRITE_DONE || done->result < 0) {
        M_DEBUG("udpfs: write error: type=0x%02x result=%d\n",
            done->msg_type, done->result);
        return done->result < 0 ? done->result : -EIO;
    }

    return done->result;
}


/*
 * Core UDPFS initialization (socket + discovery)
 */
int udpfs_core_init(void)
{
    int ret;

    M_DEBUG("UDPFS: initializing core\n");

    /* Create UDPRDMA socket */
    g_socket = udprdma_create(UDPFS_PORT, UDPRDMA_SVC_UDPFS);
    if (g_socket == NULL) {
        M_DEBUG("UDPFS: failed to create socket\n");
        return -1;
    }

    /* Discover server */
    M_DEBUG("UDPFS: discovering server...\n");
    ret = udprdma_discover(g_socket, 5000);
    if (ret != UDPRDMA_OK) {
        M_DEBUG("UDPFS: server not found\n");
        udprdma_destroy(g_socket);
        g_socket = NULL;
        return -1;
    }

    M_DEBUG("UDPFS: connected to %d.%d.%d.%d\n",
        (udprdma_get_peer_ip(g_socket) >> 24) & 0xFF,
        (udprdma_get_peer_ip(g_socket) >> 16) & 0xFF,
        (udprdma_get_peer_ip(g_socket) >>  8) & 0xFF,
        (udprdma_get_peer_ip(g_socket) >>  0) & 0xFF);

    M_DEBUG("UDPFS: ready\n");
    return 0;
}

/*
 * Core UDPFS cleanup
 */
void udpfs_core_exit(void)
{
    M_DEBUG("UDPFS: shutting down core\n");
    if (g_socket != NULL) {
        udprdma_destroy(g_socket);
        g_socket = NULL;
    }
}

/*
 * Query connection status
 */
int udpfs_core_is_connected(void)
{
    return udprdma_is_connected(g_socket);
}

/*
 * File system operations (IOMAN only)
 */
#ifdef FEATURE_UDPFS_IOMAN

/*
 * Open file or directory
 */
int udpfs_core_open(const char *path, int flags, int mode, int is_dir, int32_t *handle_out)
{
    udpfs_msg_open_req_t *req = (udpfs_msg_open_req_t *)g_tx_buf;
    udpfs_msg_open_reply_t *reply = (udpfs_msg_open_reply_t *)g_rx_buf;
    uint32_t path_len, padded;
    int ret;

    M_DEBUG("udpfs_core_open(%s, is_dir=%d)\n", path, is_dir);

    if (!udprdma_is_connected(g_socket))
        return -EIO;

    /* Strip leading slashes */
    while (path[0] == '/' || path[0] == '\\')
        path++;

    /* Build OPEN_REQ */
    path_len = strlen(path) + 1;
    if (path_len > UDPFS_MAX_PATH)
        return -ENAMETOOLONG;

    padded = _padded_len(path_len);

    req->msg_type = UDPFS_MSG_OPEN_REQ;
    req->is_dir = is_dir ? 1 : 0;
    req->flags = flags;
    req->mode = mode;
    memcpy(g_tx_buf + sizeof(udpfs_msg_open_req_t), path, path_len);
    memset(g_tx_buf + sizeof(udpfs_msg_open_req_t) + path_len, 0, padded - path_len);

    ret = _request(req, sizeof(udpfs_msg_open_req_t) + padded,
                   reply, sizeof(udpfs_msg_open_reply_t));
    if (ret < 0)
        return ret;

    if (reply->msg_type != UDPFS_MSG_OPEN_REPLY || reply->handle < 0) {
        M_DEBUG("udpfs: open failed: handle=%d\n", reply->handle);
        return reply->handle < 0 ? reply->handle : -EIO;
    }

    *handle_out = reply->handle;

    M_DEBUG("udpfs: opened '%s' -> handle=%d, size=%u\n",
        path, reply->handle, reply->size);

    return 0;
}

/*
 * Close file/directory
 */
int udpfs_core_close(int32_t handle)
{
    udpfs_msg_close_req_t *req = (udpfs_msg_close_req_t *)g_tx_buf;
    udpfs_msg_close_reply_t *reply = (udpfs_msg_close_reply_t *)g_rx_buf;

    M_DEBUG("udpfs_core_close(handle=%d)\n", handle);

    if (!udprdma_is_connected(g_socket))
        return -EIO;

    if (handle < 0)
        return -EBADF;

    req->msg_type = UDPFS_MSG_CLOSE_REQ;
    req->reserved[0] = 0;
    req->reserved[1] = 0;
    req->reserved[2] = 0;
    req->handle = handle;

    _request(req, sizeof(udpfs_msg_close_req_t),
             reply, sizeof(udpfs_msg_close_reply_t));

    return 0;
}

/*
 * Read from file
 */
int udpfs_core_read(int32_t handle, void *buffer, int size)
{
    int total_read = 0;
    int ret;

    if (!udprdma_is_connected(g_socket))
        return -EIO;

    if (handle < 0)
        return -EBADF;

    M_DEBUG("udpfs_core_read(handle=%d, %d bytes)\n", handle, size);

    while (size > 0) {
        udpfs_msg_read_req_t req;
        int chunk_size = size > UDPFS_MAX_READ ? UDPFS_MAX_READ : size;

        req.msg_type = UDPFS_MSG_READ_REQ;
        req.reserved[0] = 0;
        req.reserved[1] = 0;
        req.reserved[2] = 0;
        req.handle = handle;
        req.size = chunk_size;

        ret = _recv_with_result(g_socket, &req, sizeof(req), buffer, chunk_size, 30000);

        if (ret < 0) {
            return total_read > 0 ? total_read : -EIO;
        }

        if (ret == 0)
            break;  /* EOF */

        total_read += ret;
        buffer = (uint8_t *)buffer + ret;
        size -= ret;

        if (ret < chunk_size)
            break;  /* Short read = EOF */
    }

    return total_read;
}

/*
 * Write to file
 */
int udpfs_core_write(int32_t handle, const void *buffer, int size)
{
    udpfs_msg_write_req_t req;

    if (!udprdma_is_connected(g_socket))
        return -EIO;

    if (handle < 0)
        return -EBADF;

    M_DEBUG("udpfs_core_write(handle=%d, %d bytes)\n", handle, size);

    /* Build WRITE_REQ */
    req.msg_type = UDPFS_MSG_WRITE_REQ;
    req.reserved[0] = 0;
    req.reserved[1] = 0;
    req.reserved[2] = 0;
    req.handle = handle;
    req.size = size;

    /* Use shared write helper */
    return _write_with_combined_header(g_socket, &req, sizeof(req), buffer, size);
}

/*
 * Seek in file
 */
int64_t udpfs_core_lseek(int32_t handle, int64_t offset, int whence)
{
    udpfs_msg_lseek_req_t *req = (udpfs_msg_lseek_req_t *)g_tx_buf;
    udpfs_msg_lseek_reply_t *reply = (udpfs_msg_lseek_reply_t *)g_rx_buf;
    int ret;

    if (!udprdma_is_connected(g_socket))
        return -EIO;

    if (handle < 0)
        return -EBADF;

    M_DEBUG("udpfs_core_lseek(handle=%d, offset=0x%x%08x, whence=%d)\n",
        handle, (unsigned int)(offset >> 32), (unsigned int)offset, whence);

    /* Build LSEEK_REQ */
    req->msg_type = UDPFS_MSG_LSEEK_REQ;
    req->whence = whence;
    req->reserved = 0;
    req->handle = handle;
    req->offset_lo = (int32_t)(offset & 0xFFFFFFFF);
    req->offset_hi = (int32_t)(offset >> 32);

    ret = _request(req, sizeof(udpfs_msg_lseek_req_t),
                   reply, sizeof(udpfs_msg_lseek_reply_t));
    if (ret < 0)
        return ret;

    if (reply->msg_type != UDPFS_MSG_LSEEK_REPLY) {
        M_DEBUG("udpfs: lseek: unexpected reply type 0x%02x\n", reply->msg_type);
        return -EIO;
    }

    if (reply->position_lo < 0 && reply->position_hi == -1)
        return reply->position_lo;  /* Error code */

    return ((int64_t)(uint32_t)reply->position_hi << 32) | (uint32_t)reply->position_lo;
}

/*
 * Read directory entry
 */
int udpfs_core_dread(int32_t handle, iox_stat_t *stat, char *name, int name_max)
{
    udpfs_msg_dread_req_t *req = (udpfs_msg_dread_req_t *)g_tx_buf;
    udpfs_msg_dread_reply_t *reply = (udpfs_msg_dread_reply_t *)g_rx_buf;
    int ret;

    if (!udprdma_is_connected(g_socket))
        return -EIO;

    if (handle < 0)
        return -EBADF;

    M_DEBUG("udpfs_core_dread(handle=%d)\n", handle);

    req->msg_type = UDPFS_MSG_DREAD_REQ;
    req->reserved[0] = 0;
    req->reserved[1] = 0;
    req->reserved[2] = 0;
    req->handle = handle;

    ret = _request(req, sizeof(udpfs_msg_dread_req_t),
                   reply, sizeof(g_rx_buf));
    if (ret < 0)
        return ret;

    if (reply->msg_type != UDPFS_MSG_DREAD_REPLY) {
        M_DEBUG("udpfs: dread: unexpected reply type 0x%02x\n", reply->msg_type);
        return -EIO;
    }

    if (reply->result <= 0)
        return reply->result;  /* 0 = end of dir, negative = error */

    /* Populate stat */
    memset(stat, 0, sizeof(iox_stat_t));
    stat->mode = reply->mode;
    stat->attr = reply->attr;
    stat->size = reply->size;
    stat->hisize = reply->hisize;
    memcpy(stat->ctime, reply->ctime, 8);
    memcpy(stat->atime, reply->atime, 8);
    memcpy(stat->mtime, reply->mtime, 8);

    /* Copy name */
    if (reply->name_len > 0 && reply->name_len < name_max) {
        memcpy(name, (char *)reply + sizeof(udpfs_msg_dread_reply_t), reply->name_len);
        name[reply->name_len] = '\0';
    } else if (name_max > 0) {
        name[0] = '\0';
    }

    return 1;
}

/*
 * Get file/directory statistics
 */
int udpfs_core_getstat(const char *path, iox_stat_t *stat)
{
    udpfs_msg_getstat_req_t *req = (udpfs_msg_getstat_req_t *)g_tx_buf;
    udpfs_msg_getstat_reply_t *reply = (udpfs_msg_getstat_reply_t *)g_rx_buf;
    uint32_t path_len, padded;
    int ret;

    M_DEBUG("udpfs_core_getstat(%s)\n", path);

    if (!udprdma_is_connected(g_socket))
        return -EIO;

    while (path[0] == '/' || path[0] == '\\')
        path++;

    path_len = strlen(path) + 1;
    if (path_len > UDPFS_MAX_PATH)
        return -ENAMETOOLONG;
    padded = _padded_len(path_len);

    req->msg_type = UDPFS_MSG_GETSTAT_REQ;
    req->reserved[0] = 0;
    req->reserved[1] = 0;
    req->reserved[2] = 0;
    memcpy(g_tx_buf + sizeof(udpfs_msg_getstat_req_t), path, path_len);
    memset(g_tx_buf + sizeof(udpfs_msg_getstat_req_t) + path_len, 0, padded - path_len);

    ret = _request(req, sizeof(udpfs_msg_getstat_req_t) + padded,
                   reply, sizeof(udpfs_msg_getstat_reply_t));
    if (ret < 0)
        return ret;

    if (reply->msg_type != UDPFS_MSG_GETSTAT_REPLY) {
        M_DEBUG("udpfs: getstat: unexpected reply type 0x%02x\n", reply->msg_type);
        return -EIO;
    }

    if (reply->result < 0)
        return reply->result;

    memset(stat, 0, sizeof(iox_stat_t));
    stat->mode = reply->mode;
    stat->attr = reply->attr;
    stat->size = reply->size;
    stat->hisize = reply->hisize;
    memcpy(stat->ctime, reply->ctime, 8);
    memcpy(stat->atime, reply->atime, 8);
    memcpy(stat->mtime, reply->mtime, 8);

    return 0;
}

/*
 * Create directory
 */
int udpfs_core_mkdir(const char *path, int mode)
{
    M_DEBUG("udpfs_core_mkdir(%s, 0x%x)\n", path, mode);
    return _path_op(UDPFS_MSG_MKDIR_REQ, path, mode);
}

/*
 * Remove file
 */
int udpfs_core_remove(const char *path)
{
    M_DEBUG("udpfs_core_remove(%s)\n", path);
    return _path_op(UDPFS_MSG_REMOVE_REQ, path, 0);
}

/*
 * Remove directory
 */
int udpfs_core_rmdir(const char *path)
{
    M_DEBUG("udpfs_core_rmdir(%s)\n", path);
    return _path_op(UDPFS_MSG_RMDIR_REQ, path, 0);
}

#endif /* FEATURE_UDPFS_IOMAN */

/*
 * Block device operations (BD and FHI)
 */
#if defined(FEATURE_UDPFS_BD) || defined(FEATURE_UDPFS_FHI)

/*
 * Block read with retry and chunking.
 * Splits large reads into UDPFS_MAX_SECTOR_READ chunks.
 * Returns sector count on success, negative on error.
 */
int udpfs_core_bread(int32_t handle, uint64_t sector, void *buffer, uint32_t count, uint32_t sector_size)
{
    uint32_t sectors_left = count;

    M_DEBUG("udpfs_core_bread(handle=%d, sector=%u, count=%u)\n",
        handle, (unsigned int)sector, count);

    if (!udprdma_is_connected(g_socket))
        return -EIO;

    if (handle < 0)
        return -EBADF;

    while (sectors_left > 0) {
        udpfs_msg_bread_req_t req;
        uint16_t chunk = sectors_left > UDPFS_MAX_SECTOR_READ ?
            UDPFS_MAX_SECTOR_READ : (uint16_t)sectors_left;
        int ret;

        req.msg_type = UDPFS_MSG_BREAD_REQ;
        req.reserved = 0;
        req.sector_count = chunk;
        req.handle = handle;
        req.sector_nr_lo = (uint32_t)(sector & 0xFFFFFFFF);
        req.sector_nr_hi = (uint32_t)(sector >> 32);

        ret = _recv_with_result(g_socket, &req, sizeof(req), buffer, chunk * sector_size, 30000);

        if (ret < 0) {
            M_DEBUG("udpfs: bread failed\n");
            return -EIO;
        }

        sectors_left -= chunk;
        sector += chunk;
        buffer = (uint8_t *)buffer + chunk * sector_size;
    }

    return count;
}

/*
 * Full block write: send BWRITE_REQ + write data chunks + wait WRITE_DONE.
 * Returns sector count on success, negative on error.
 */
int udpfs_core_bwrite(int32_t handle, uint64_t sector, const void *buffer, uint32_t count, uint32_t sector_size)
{
    udpfs_msg_bwrite_req_t req;
    uint32_t total_size = count * sector_size;
    int ret;

    M_DEBUG("udpfs_core_bwrite(handle=%d, sector=%u, count=%u)\n",
        handle, (unsigned int)sector, count);

    if (!udprdma_is_connected(g_socket))
        return -EIO;

    if (handle < 0)
        return -EBADF;

    /* Build BWRITE_REQ */
    req.msg_type = UDPFS_MSG_BWRITE_REQ;
    req.reserved = 0;
    req.sector_count = (uint16_t)count;
    req.handle = handle;
    req.sector_nr_lo = (uint32_t)(sector & 0xFFFFFFFF);
    req.sector_nr_hi = (uint32_t)(sector >> 32);

    /* Use shared write helper */
    ret = _write_with_combined_header(g_socket, &req, sizeof(req), buffer, total_size);
    if (ret < 0)
        return ret;

    return count;
}

/*
 * Query block device capacity via GETSTAT
 * Sectors are fixed at 512 bytes.
 * Returns sector count on success, negative on error.
 */
int udpfs_core_get_sector_count(int32_t handle)
{
    udpfs_msg_getstat_req_t req __attribute__((aligned(4)));
    udpfs_msg_getstat_reply_t reply __attribute__((aligned(4)));
    int ret;

    M_DEBUG("udpfs_core_get_sector_count(handle=%d)\n", handle);

    if (!udprdma_is_connected(g_socket))
        return -EIO;

    if (handle < 0)
        return -EBADF;

    /* For block device, query via empty path GETSTAT */
    req.msg_type = UDPFS_MSG_GETSTAT_REQ;
    req.reserved[0] = 0;
    req.reserved[1] = 0;
    req.reserved[2] = 0;

    udprdma_set_rx_buffer(g_socket, &reply, sizeof(reply));

    ret = udprdma_send(g_socket, &req, sizeof(req));
    if (ret != UDPRDMA_OK) {
        M_DEBUG("udpfs: sector_count send failed: %d\n", ret);
        return -EIO;
    }

    ret = udprdma_recv(g_socket, &reply, sizeof(reply), 5000);
    if (ret < 0) {
        M_DEBUG("udpfs: sector_count recv failed: %d\n", ret);
        return -EIO;
    }

    if (reply.msg_type != UDPFS_MSG_GETSTAT_REPLY) {
        M_DEBUG("udpfs: unexpected reply type 0x%02x\n", reply.msg_type);
        return -EIO;
    }

    if (reply.result < 0) {
        M_DEBUG("udpfs: getstat error: %d\n", reply.result);
        return reply.result;
    }

    /* Sector count = total bytes / 512 */
    uint64_t total_bytes = (uint64_t)reply.size | ((uint64_t)reply.hisize << 32);
    uint32_t sector_count = (uint32_t)(total_bytes >> 9);  /* Divide by 512 */

    M_DEBUG("udpfs: block device: %u bytes, %u sectors\n", (unsigned int)total_bytes, sector_count);

    return sector_count;
}

#endif /* FEATURE_UDPFS_BD || FEATURE_UDPFS_FHI */
