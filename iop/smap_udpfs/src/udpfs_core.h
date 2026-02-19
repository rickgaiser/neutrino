#ifndef UDPFS_CORE_H
#define UDPFS_CORE_H

/*
 * UDPFS Core - File System Protocol over UDPRDMA
 *
 * Core protocol implementation for UDPFS.
 * Provides low-level file and block I/O operations using server_handle directly.
 * All operations use UDPRDMA for reliable transport.
 */

#include <stdint.h>
#include <iomanX.h>

/* Initialize core UDPFS (socket + discovery) */
int udpfs_core_init(void);

/* Cleanup core UDPFS */
void udpfs_core_exit(void);

/* Query connection status (returns 1 if connected, 0 otherwise) */
int udpfs_core_is_connected(void);

/* Open file or directory
 * is_dir: 1 to open as directory, 0 for file
 * Returns server handle via handle_out on success, negative errno on error
 */
int udpfs_core_open(const char *path, int flags, int mode, int is_dir, int32_t *handle_out);

/* Close file/directory */
int udpfs_core_close(int32_t handle);

/* Read from file
 * Returns bytes read on success, 0 for EOF, negative errno on error
 */
int udpfs_core_read(int32_t handle, void *buffer, int size);

/* Write to file
 * Returns bytes written on success, negative errno on error
 */
int udpfs_core_write(int32_t handle, const void *buffer, int size);

/* Seek in file
 * Returns new offset on success, negative errno on error
 */
int64_t udpfs_core_lseek(int32_t handle, int64_t offset, int whence);

/* Read directory entry
 * Populates stat and name buffers
 * Returns 1 if entry read, 0 for end of directory, negative errno on error
 */
int udpfs_core_dread(int32_t handle, iox_stat_t *stat, char *name, int name_max);

/* Get file/directory statistics */
int udpfs_core_getstat(const char *path, iox_stat_t *stat);

/* Create directory */
int udpfs_core_mkdir(const char *path, int mode);

/* Remove file */
int udpfs_core_remove(const char *path);

/* Remove directory */
int udpfs_core_rmdir(const char *path);

/* Block read with retry and chunking
 * Returns sector count on success, negative errno on error
 */
int udpfs_core_bread(int32_t handle, uint64_t sector, void *buffer, uint32_t count, uint32_t sector_size);

/* Block write with retry and chunking
 * Returns sector count on success, negative errno on error
 */
int udpfs_core_bwrite(int32_t handle, uint64_t sector, const void *buffer, uint32_t count, uint32_t sector_size);

/* Query block device capacity via GETSTAT
 * Returns sector count on success (sectors = 512 bytes), negative errno on error
 */
int udpfs_core_get_sector_count(int32_t handle);

#endif /* UDPFS_CORE_H */
