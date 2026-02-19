#ifndef UDPFS_PACKET_H
#define UDPFS_PACKET_H

/*
 * UDPFS - File System Protocol over UDPRDMA
 *
 * Provides file-level and block-level access from PS2 IOP to a PC server.
 * UDPBD is a subset of this protocol using only block I/O messages.
 *
 * File operations (full UDPFS):
 * - Open/Close, Read/Write, Lseek, Dread, Getstat, Mkdir/Remove/Rmdir
 *
 * Block I/O operations (UDPBD subset):
 * - BREAD/BWRITE: sector-addressed read/write on any file handle
 * - Block device capacity is queried via GETSTAT (size field = total bytes)
 * - Handle 0 is reserved for the block device (pre-opened by server)
 * - Sectors are fixed at 512 bytes
 *
 * All operations use file handles. Block devices and files are interchangeable.
 *
 * All structures are packed and 4-byte aligned for DMA compatibility.
 * Paths are null-terminated, padded to 4-byte boundary.
 */

#include <stdint.h>


/* Message types */
#define UDPFS_MSG_OPEN_REQ      0x10  /* Open file or directory */
#define UDPFS_MSG_OPEN_REPLY    0x11  /* Open response (handle + stat) */
#define UDPFS_MSG_CLOSE_REQ     0x12  /* Close handle */
#define UDPFS_MSG_CLOSE_REPLY   0x13  /* Close response */
#define UDPFS_MSG_READ_REQ      0x14  /* Read from file */
/* 0x15 reserved - read response is RESULT_REPLY + raw data */
#define UDPFS_MSG_WRITE_REQ     0x16  /* Write to file */
#define UDPFS_MSG_WRITE_DATA    0x17  /* Write data chunk */
#define UDPFS_MSG_WRITE_DONE    0x18  /* Write completion */
#define UDPFS_MSG_LSEEK_REQ     0x1A  /* Seek in file */
#define UDPFS_MSG_LSEEK_REPLY   0x1B  /* Seek response (new position) */
#define UDPFS_MSG_DREAD_REQ     0x1C  /* Read directory entry */
#define UDPFS_MSG_DREAD_REPLY   0x1D  /* Directory entry response */
#define UDPFS_MSG_GETSTAT_REQ   0x1E  /* Get file stats */
#define UDPFS_MSG_GETSTAT_REPLY 0x1F  /* Stats response */
#define UDPFS_MSG_MKDIR_REQ     0x20  /* Create directory */
#define UDPFS_MSG_REMOVE_REQ    0x22  /* Remove file */
#define UDPFS_MSG_RMDIR_REQ     0x24  /* Remove directory */
#define UDPFS_MSG_RESULT_REPLY  0x26  /* Generic result */

/* Block I/O message types (UDPBD subset) */
#define UDPFS_MSG_BREAD_REQ     0x28  /* Block read request */
/* 0x29 reserved - response uses RESULT_REPLY app header + raw data */
#define UDPFS_MSG_BWRITE_REQ    0x2A  /* Block write request */
/* BWRITE uses WRITE_DATA (0x17) for chunks and WRITE_DONE (0x18) for completion */

/* Limits */
#define UDPFS_MAX_PATH          256   /* Max path length including null */
#define UDPFS_MAX_READ          (64 * 1024)  /* Max bytes per read request */
#define UDPFS_MAX_PAYLOAD       1400  /* Max message payload */
#define UDPFS_MAX_RETRIES       4
#define UDPFS_MAX_SECTOR_READ   128   /* Max sectors per block read (64KB) */


/*
 * Message structures
 * All structures are packed and 4-byte aligned for DMA compatibility.
 * Paths are null-terminated, padded to 4-byte boundary.
 */

/* Open request (variable length: 8 + path) */
typedef struct {
    uint8_t  msg_type;     /* UDPFS_MSG_OPEN_REQ */
    uint8_t  is_dir;       /* 1 = dopen, 0 = open */
    uint16_t flags;        /* O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, O_TRUNC */
    int32_t  mode;         /* File creation mode (for O_CREAT) */
    /* char path[] follows - null-terminated, padded to 4-byte boundary */
} __attribute__((packed, aligned(4))) udpfs_msg_open_req_t;

/* Open reply (36 bytes) */
typedef struct {
    uint8_t  msg_type;     /* UDPFS_MSG_OPEN_REPLY */
    uint8_t  reserved[3];
    int32_t  handle;       /* >= 0 = server handle, < 0 = -errno */
    uint32_t mode;         /* FIO_S_IFREG / FIO_S_IFDIR */
    uint32_t size;         /* File size low 32 bits */
    uint32_t hisize;       /* File size high 32 bits */
    uint8_t  ctime[8];     /* Creation time (PS2 format) */
    uint8_t  mtime[8];     /* Modification time (PS2 format) */
} __attribute__((packed, aligned(4))) udpfs_msg_open_reply_t;

/* Close request (8 bytes) */
typedef struct {
    uint8_t  msg_type;     /* UDPFS_MSG_CLOSE_REQ */
    uint8_t  reserved[3];
    int32_t  handle;       /* Server handle */
} __attribute__((packed, aligned(4))) udpfs_msg_close_req_t;

/* Close reply (8 bytes) */
typedef struct {
    uint8_t  msg_type;     /* UDPFS_MSG_CLOSE_REPLY */
    uint8_t  reserved[3];
    int32_t  result;       /* 0 = success, negative = -errno */
} __attribute__((packed, aligned(4))) udpfs_msg_close_reply_t;

/* Read request (12 bytes) */
typedef struct {
    uint8_t  msg_type;     /* UDPFS_MSG_READ_REQ */
    uint8_t  reserved[3];
    int32_t  handle;       /* Server handle */
    uint32_t size;         /* Bytes to read (max UDPFS_MAX_READ) */
} __attribute__((packed, aligned(4))) udpfs_msg_read_req_t;

/* Read response: RESULT_REPLY (bytes_read) followed by raw data via UDPRDMA */

/* Write request (12 bytes) */
typedef struct {
    uint8_t  msg_type;     /* UDPFS_MSG_WRITE_REQ */
    uint8_t  reserved[3];
    int32_t  handle;       /* Server handle */
    uint32_t size;         /* Total bytes to write */
} __attribute__((packed, aligned(4))) udpfs_msg_write_req_t;

/* Write data chunk (8 bytes + data, same pattern as UDPBD) */
typedef struct {
    uint8_t  msg_type;     /* UDPFS_MSG_WRITE_DATA */
    uint8_t  reserved;
    uint16_t chunk_nr;     /* Chunk number (0, 1, 2, ...) */
    uint16_t chunk_size;   /* Size of data in this chunk */
    uint16_t total_chunks; /* Total chunks for this write */
    /* uint8_t data[] follows */
} __attribute__((packed, aligned(4))) udpfs_msg_write_data_t;

/* Write done (8 bytes) */
typedef struct {
    uint8_t  msg_type;     /* UDPFS_MSG_WRITE_DONE */
    uint8_t  reserved[3];
    int32_t  result;       /* Bytes written (>= 0) or -errno */
} __attribute__((packed, aligned(4))) udpfs_msg_write_done_t;

/* Lseek request (16 bytes) */
typedef struct {
    uint8_t  msg_type;     /* UDPFS_MSG_LSEEK_REQ */
    uint8_t  whence;       /* SEEK_SET=0, SEEK_CUR=1, SEEK_END=2 */
    uint16_t reserved;
    int32_t  handle;       /* Server handle */
    int32_t  offset_lo;    /* Offset low 32 bits */
    int32_t  offset_hi;    /* Offset high 32 bits */
} __attribute__((packed, aligned(4))) udpfs_msg_lseek_req_t;

/* Lseek reply (12 bytes) */
typedef struct {
    uint8_t  msg_type;     /* UDPFS_MSG_LSEEK_REPLY */
    uint8_t  reserved[3];
    int32_t  position_lo;  /* New position low 32 bits (or -errno) */
    int32_t  position_hi;  /* New position high 32 bits */
} __attribute__((packed, aligned(4))) udpfs_msg_lseek_reply_t;

/* Directory read request (8 bytes) */
typedef struct {
    uint8_t  msg_type;     /* UDPFS_MSG_DREAD_REQ */
    uint8_t  reserved[3];
    int32_t  handle;       /* Server directory handle */
} __attribute__((packed, aligned(4))) udpfs_msg_dread_req_t;

/* Directory read reply (variable: 48 + name_len padded) */
typedef struct {
    uint8_t  msg_type;     /* UDPFS_MSG_DREAD_REPLY */
    uint8_t  reserved;
    uint16_t name_len;     /* Name length (0 = end of dir) */
    int32_t  result;       /* 1=entry valid, 0=end-of-dir, <0=error */
    uint32_t mode;         /* FIO_S_IFREG / FIO_S_IFDIR */
    uint32_t attr;
    uint32_t size;         /* File size low 32 bits */
    uint32_t hisize;       /* File size high 32 bits */
    uint8_t  ctime[8];     /* Creation time */
    uint8_t  atime[8];     /* Access time */
    uint8_t  mtime[8];     /* Modification time */
    /* char name[] follows - null-terminated, padded to 4-byte boundary */
} __attribute__((packed, aligned(4))) udpfs_msg_dread_reply_t;

/* Getstat request (variable: 4 + path) */
typedef struct {
    uint8_t  msg_type;     /* UDPFS_MSG_GETSTAT_REQ */
    uint8_t  reserved[3];
    /* char path[] follows - null-terminated, padded to 4-byte boundary */
} __attribute__((packed, aligned(4))) udpfs_msg_getstat_req_t;

/* Getstat reply (48 bytes) */
typedef struct {
    uint8_t  msg_type;     /* UDPFS_MSG_GETSTAT_REPLY */
    uint8_t  reserved[3];
    int32_t  result;       /* 0=success, <0=error */
    uint32_t mode;
    uint32_t attr;
    uint32_t size;         /* File size low 32 bits */
    uint32_t hisize;       /* File size high 32 bits */
    uint8_t  ctime[8];
    uint8_t  atime[8];
    uint8_t  mtime[8];
} __attribute__((packed, aligned(4))) udpfs_msg_getstat_reply_t;

/* Path-based request for mkdir/remove/rmdir (variable: 4 + path) */
typedef struct {
    uint8_t  msg_type;     /* UDPFS_MSG_MKDIR_REQ / REMOVE_REQ / RMDIR_REQ */
    uint8_t  reserved;
    uint16_t mode;         /* For mkdir: creation mode. Unused for remove/rmdir. */
    /* char path[] follows - null-terminated, padded to 4-byte boundary */
} __attribute__((packed, aligned(4))) udpfs_msg_path_req_t;

/* Generic result reply (8 bytes) */
typedef struct {
    uint8_t  msg_type;     /* UDPFS_MSG_RESULT_REPLY */
    uint8_t  reserved[3];
    int32_t  result;       /* 0=success, positive=value, negative=-errno */
} __attribute__((packed, aligned(4))) udpfs_msg_result_reply_t;


/*
 * Block I/O message structures (UDPBD subset)
 */

/* Block read request (16 bytes) */
typedef struct {
    uint8_t  msg_type;      /* UDPFS_MSG_BREAD_REQ */
    uint8_t  reserved;      /* Padding */
    uint16_t sector_count;  /* Number of sectors to read (max UDPFS_MAX_SECTOR_READ) */
    int32_t  handle;        /* File handle (0 = block device) */
    uint32_t sector_nr_lo;  /* Sector number (low 32 bits) */
    uint32_t sector_nr_hi;  /* Sector number (high 32 bits) */
} __attribute__((packed, aligned(4))) udpfs_msg_bread_req_t;
/* Response: RESULT_REPLY app header + raw sector data via UDPRDMA (same as file read) */

/* Block write request (16 bytes) */
typedef struct {
    uint8_t  msg_type;      /* UDPFS_MSG_BWRITE_REQ */
    uint8_t  reserved;      /* Padding */
    uint16_t sector_count;  /* Number of sectors to write */
    int32_t  handle;        /* File handle (0 = block device) */
    uint32_t sector_nr_lo;  /* Sector number (low 32 bits) */
    uint32_t sector_nr_hi;  /* Sector number (high 32 bits) */
} __attribute__((packed, aligned(4))) udpfs_msg_bwrite_req_t;
/* Followed by WRITE_DATA (0x17) chunks, server responds with WRITE_DONE (0x18) */


#endif /* UDPFS_PACKET_H */
