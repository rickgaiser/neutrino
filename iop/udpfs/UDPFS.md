# UDPFS Protocol Specification

**UDP File System** - A remote file system and block device access protocol for the PS2 IOP. Provides file-level operations (open, read, write, seek, directory listing, stat) and block-level I/O over UDPRDMA reliable transport.

## Design Goals

- **Unified**: Single protocol for both file system and block device access
- **Zero-copy reads**: File and sector data DMA'd directly into caller's buffer
- **Handle-based**: Server-side file handles allow multiple concurrent open files
- **UDPBD compatible**: Block I/O subset provides sector-level access via same connection
- **Leverages UDPRDMA**: All reliability, discovery, and DMA handling delegated to transport layer

## Protocol Stack

```
┌─────────────────────────────────┐
│  iomanX "udpfs:" / BDM "udp"    │
├─────────────────────────────────┤
│  UDPFS (this protocol)          │
├─────────────────────────────────┤
│  UDPRDMA (reliable transport)   │
├─────────────────────────────────┤
│  UDP / IP / Ethernet            │
└─────────────────────────────────┘
```

## Service Discovery

UDPFS uses UDPRDMA service discovery with service ID **0xF5F5**:

```
PS2 (client)                     PC (server)
   |                                |
   |-- UDPRDMA DISCOVERY ---------->|  service_id=0xF5F5
   |<--------- UDPRDMA INFORM ------|  service_id=0xF5F5
   |                                |
   [UDPRDMA connection established]
   [UDPFS ready]
```

After connection, the client can immediately use file operations. For block device mode (UDPBD), a GETSTAT with empty path is sent first to get disk capacity before registering the block device.

## Message Types

### File Operations

| Type | Value | Direction | Description |
|------|-------|-----------|-------------|
| OPEN_REQ | 0x10 | Client -> Server | Open file or directory |
| OPEN_REPLY | 0x11 | Server -> Client | Open response (handle + stat) |
| CLOSE_REQ | 0x12 | Client -> Server | Close handle |
| CLOSE_REPLY | 0x13 | Server -> Client | Close response |
| READ_REQ | 0x14 | Client -> Server | Read from file |
| WRITE_REQ | 0x16 | Client -> Server | Write to file |
| WRITE_DATA | 0x17 | Client -> Server | Write data chunk |
| WRITE_DONE | 0x18 | Server -> Client | Write completion |
| LSEEK_REQ | 0x1A | Client -> Server | Seek in file |
| LSEEK_REPLY | 0x1B | Server -> Client | Seek response (new position) |
| DREAD_REQ | 0x1C | Client -> Server | Read directory entry |
| DREAD_REPLY | 0x1D | Server -> Client | Directory entry response |
| GETSTAT_REQ | 0x1E | Client -> Server | Get file stats |
| GETSTAT_REPLY | 0x1F | Server -> Client | Stats response |
| MKDIR_REQ | 0x20 | Client -> Server | Create directory |
| REMOVE_REQ | 0x22 | Client -> Server | Remove file |
| RMDIR_REQ | 0x24 | Client -> Server | Remove directory |
| RESULT_REPLY | 0x26 | Server -> Client | Generic result |

Note: 0x15 (READ_REPLY) is reserved — read responses use RESULT_REPLY as UDPRDMA app header + raw data.

### Block I/O Operations (UDPBD Subset)

| Type | Value | Direction | Description |
|------|-------|-----------|-------------|
| BREAD_REQ | 0x28 | Client -> Server | Block read request |
| BWRITE_REQ | 0x2A | Client -> Server | Block write request |

Note: BREAD responses use the same RESULT_REPLY app header + raw data pattern as file reads. BWRITE uses WRITE_DATA (0x17) for chunks and WRITE_DONE (0x18) for completion.

## Handle Model

The server manages file handles as integers:

| Handle | Description |
|--------|-------------|
| 0 | Reserved for block device (pre-opened by server) |
| 1+ | Dynamically allocated for open files/directories |

- Handle 0 is always the block device image (if configured)
- Handle 0 cannot be closed
- Maximum simultaneous handles: 8 (client-side) / 32 (server-side)

## Message Structures

All messages start with a `msg_type` byte. Structures are packed and 4-byte aligned for DMA compatibility. Paths are null-terminated and padded to 4-byte boundary.

### OPEN_REQ (8 bytes + path)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   msg_type    |    is_dir     |            flags              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           mode                                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      path (variable) ...                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| Field | Bytes | Description |
|-------|-------|-------------|
| msg_type | 1 | 0x10 (OPEN_REQ) |
| is_dir | 1 | 1 = dopen (directory), 0 = open (file) |
| flags | 2 | Open flags (see below) |
| mode | 4 | File creation mode (for O_CREAT), signed |
| path | var | Null-terminated, padded to 4-byte boundary |

**Open flags:**

| Flag | Value | Description |
|------|-------|-------------|
| O_RDONLY | 0x0001 | Read only |
| O_WRONLY | 0x0002 | Write only |
| O_RDWR | 0x0003 | Read/write |
| O_APPEND | 0x0100 | Append mode |
| O_CREAT | 0x0200 | Create if not exists |
| O_TRUNC | 0x0400 | Truncate to zero length |

### OPEN_REPLY (36 bytes)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   msg_type    |                  reserved                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         handle                                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          mode                                 |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          size                                 |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         hisize                                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       ctime (8 bytes)                         |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       mtime (8 bytes)                         |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| Field | Bytes | Description |
|-------|-------|-------------|
| msg_type | 1 | 0x11 (OPEN_REPLY) |
| reserved | 3 | Must be 0 |
| handle | 4 | >= 0: server handle, < 0: -errno (signed) |
| mode | 4 | FIO_S_IFREG (0x2000) or FIO_S_IFDIR (0x1000) |
| size | 4 | File size low 32 bits |
| hisize | 4 | File size high 32 bits |
| ctime | 8 | Creation time (PS2 format) |
| mtime | 8 | Modification time (PS2 format) |

### CLOSE_REQ (8 bytes)

| Field | Bytes | Description |
|-------|-------|-------------|
| msg_type | 1 | 0x12 (CLOSE_REQ) |
| reserved | 3 | Must be 0 |
| handle | 4 | Server handle to close |

### CLOSE_REPLY (8 bytes)

| Field | Bytes | Description |
|-------|-------|-------------|
| msg_type | 1 | 0x13 (CLOSE_REPLY) |
| reserved | 3 | Must be 0 |
| result | 4 | 0 = success, negative = -errno (signed) |

### READ_REQ (12 bytes)

| Field | Bytes | Description |
|-------|-------|-------------|
| msg_type | 1 | 0x14 (READ_REQ) |
| reserved | 3 | Must be 0 |
| handle | 4 | Server handle |
| size | 4 | Bytes to read (max 65536) |

### Read Response (combined)

The server sends RESULT_REPLY as an UDPRDMA app header combined with raw data in a single transfer:

- **App header**: RESULT_REPLY (8 bytes) with `result` = bytes_read (or negative error)
- **Data**: Raw file data DMA'd directly into the caller's buffer

The RESULT_REPLY is received via PIO into a local struct, while data is DMA'd to the user buffer. For EOF (bytes_read=0) or errors, only the RESULT_REPLY header is sent with no data payload.

### WRITE_REQ (12 bytes)

| Field | Bytes | Description |
|-------|-------|-------------|
| msg_type | 1 | 0x16 (WRITE_REQ) |
| reserved | 3 | Must be 0 |
| handle | 4 | Server handle |
| size | 4 | Total bytes to write |

### WRITE_DATA (8 bytes + data)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   msg_type    |   reserved    |          chunk_nr             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          chunk_size           |        total_chunks           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          data ...                             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| Field | Bytes | Description |
|-------|-------|-------------|
| msg_type | 1 | 0x17 (WRITE_DATA) |
| reserved | 1 | Must be 0 |
| chunk_nr | 2 | Chunk index (0, 1, 2, ...) |
| chunk_size | 2 | Data bytes in this chunk |
| total_chunks | 2 | Total chunks for this write |
| data | var | File/sector data (up to 1392 bytes) |

Shared between file writes (WRITE_REQ) and block writes (BWRITE_REQ).

### WRITE_DONE (8 bytes)

| Field | Bytes | Description |
|-------|-------|-------------|
| msg_type | 1 | 0x18 (WRITE_DONE) |
| reserved | 3 | Must be 0 |
| result | 4 | Bytes written (>= 0) or -errno (signed) |

### LSEEK_REQ (16 bytes)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   msg_type    |    whence     |          reserved             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         handle                                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       offset_lo                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       offset_hi                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| Field | Bytes | Description |
|-------|-------|-------------|
| msg_type | 1 | 0x1A (LSEEK_REQ) |
| whence | 1 | SEEK_SET=0, SEEK_CUR=1, SEEK_END=2 |
| reserved | 2 | Must be 0 |
| handle | 4 | Server handle |
| offset_lo | 4 | Offset low 32 bits (signed) |
| offset_hi | 4 | Offset high 32 bits (signed) |

### LSEEK_REPLY (12 bytes)

| Field | Bytes | Description |
|-------|-------|-------------|
| msg_type | 1 | 0x1B (LSEEK_REPLY) |
| reserved | 3 | Must be 0 |
| position_lo | 4 | New position low 32 bits (or -errno if error) |
| position_hi | 4 | New position high 32 bits (-1 if error) |

### DREAD_REQ (8 bytes)

| Field | Bytes | Description |
|-------|-------|-------------|
| msg_type | 1 | 0x1C (DREAD_REQ) |
| reserved | 3 | Must be 0 |
| handle | 4 | Server directory handle |

### DREAD_REPLY (48 bytes + name)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   msg_type    |   reserved    |          name_len             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         result                                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          mode                                 |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          attr                                 |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          size                                 |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         hisize                                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       ctime (8 bytes)                         |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       atime (8 bytes)                         |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       mtime (8 bytes)                         |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      name (variable) ...                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| Field | Bytes | Description |
|-------|-------|-------------|
| msg_type | 1 | 0x1D (DREAD_REPLY) |
| reserved | 1 | Must be 0 |
| name_len | 2 | Name length (0 = end of dir, excludes null terminator) |
| result | 4 | 1 = entry valid, 0 = end of dir, < 0 = error (signed) |
| mode | 4 | FIO_S_IFREG / FIO_S_IFDIR |
| attr | 4 | File attributes |
| size | 4 | File size low 32 bits |
| hisize | 4 | File size high 32 bits |
| ctime | 8 | Creation time |
| atime | 8 | Access time |
| mtime | 8 | Modification time |
| name | var | Null-terminated, padded to 4-byte boundary (only if result > 0) |

### GETSTAT_REQ (4 bytes + path)

| Field | Bytes | Description |
|-------|-------|-------------|
| msg_type | 1 | 0x1E (GETSTAT_REQ) |
| reserved | 3 | Must be 0 |
| path | var | Null-terminated, padded to 4-byte boundary |

### GETSTAT_REPLY (48 bytes)

| Field | Bytes | Description |
|-------|-------|-------------|
| msg_type | 1 | 0x1F (GETSTAT_REPLY) |
| reserved | 3 | Must be 0 |
| result | 4 | 0 = success, < 0 = error (signed) |
| mode | 4 | FIO_S_IFREG / FIO_S_IFDIR |
| attr | 4 | File attributes |
| size | 4 | File size low 32 bits |
| hisize | 4 | File size high 32 bits |
| ctime | 8 | Creation time |
| atime | 8 | Access time |
| mtime | 8 | Modification time |

### MKDIR_REQ / REMOVE_REQ / RMDIR_REQ (4 bytes + path)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   msg_type    |   reserved    |            mode               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      path (variable) ...                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| Field | Bytes | Description |
|-------|-------|-------------|
| msg_type | 1 | 0x20 (MKDIR), 0x22 (REMOVE), or 0x24 (RMDIR) |
| reserved | 1 | Must be 0 |
| mode | 2 | Creation mode (MKDIR only, unused for REMOVE/RMDIR) |
| path | var | Null-terminated, padded to 4-byte boundary |

Response: RESULT_REPLY (0x26).

### RESULT_REPLY (8 bytes)

| Field | Bytes | Description |
|-------|-------|-------------|
| msg_type | 1 | 0x26 (RESULT_REPLY) |
| reserved | 3 | Must be 0 |
| result | 4 | 0 = success, positive = value, negative = -errno (signed) |

Used as generic response for mkdir, remove, rmdir, and as the app header for file read responses (result = bytes_read).

### BREAD_REQ (16 bytes)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   msg_type    |   reserved    |         sector_count          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         handle                                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       sector_nr_lo                            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       sector_nr_hi                            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| Field | Bytes | Description |
|-------|-------|-------------|
| msg_type | 1 | 0x28 (BREAD_REQ) |
| reserved | 1 | Must be 0 |
| sector_count | 2 | Number of sectors to read (max 128) |
| handle | 4 | File handle (0 = block device), signed |
| sector_nr_lo | 4 | Starting sector number (low 32 bits) |
| sector_nr_hi | 4 | Starting sector number (high 32 bits) |

Response: RESULT_REPLY (8 bytes) as UDPRDMA app header + raw sector data, same pattern as file reads. Data is DMA'd directly into the caller's buffer.

### BWRITE_REQ (16 bytes)

Same structure as BREAD_REQ:

| Field | Bytes | Description |
|-------|-------|-------------|
| msg_type | 1 | 0x2A (BWRITE_REQ) |
| reserved | 1 | Must be 0 |
| sector_count | 2 | Number of sectors to write |
| handle | 4 | File handle (0 = block device), signed |
| sector_nr_lo | 4 | Starting sector number (low 32 bits) |
| sector_nr_hi | 4 | Starting sector number (high 32 bits) |

Followed by WRITE_DATA (0x17) chunks. Server responds with WRITE_DONE (0x18).

Block device capacity is queried via GETSTAT_REQ with empty path (no path bytes after the header). The server responds with GETSTAT_REPLY containing the block device file size in the size/hisize fields. The client divides by 512 to get the sector count.

## PS2 Time Format

Time fields (ctime, atime, mtime) use the PS2 8-byte format:

| Offset | Size | Field |
|--------|------|-------|
| 0 | 1 | unused (0) |
| 1 | 1 | seconds (0-59) |
| 2 | 1 | minutes (0-59) |
| 3 | 1 | hours (0-23) |
| 4 | 1 | day (1-31) |
| 5 | 1 | month (1-12) |
| 6 | 2 | year (little-endian) |

## Protocol Flows

### File Open/Close

```
PS2 (client)                     PC (server)
   |                                |
   |-- OPEN_REQ ------------------->|  path, flags, mode
   |   (8 + path bytes)             |
   |                                |
   |<--------- OPEN_REPLY ----------|  handle, stat
   |   (36 bytes)                   |
   |                                |
   ...
   |                                |
   |-- CLOSE_REQ ------------------>|  handle
   |<--------- CLOSE_REPLY ---------|  result
```

### File Read

```
PS2 (client)                     PC (server)
   |                                |
   |-- READ_REQ ------------------->|  handle, size
   |   (12 bytes)                   |
   |<--------- UDPRDMA ACK ---------|  immediate ACK (request received)
   |                                |  [server reads file]
   |<--- [RESULT_REPLY + data       |  RESULT_REPLY(8) as app header,
   |      chunk 0] -----------------|  raw data as payload
   |<--- raw data [chunk 1] --------|  UDPRDMA multi-packet
   |     ...                        |
   |<--- raw data [chunk K, FIN] ---|
   |                                |
   |-- UDPRDMA ACK ---------------->|
```

The server sends RESULT_REPLY (8 bytes, containing bytes_read) as an UDPRDMA app header combined with the first chunk of raw data. The client receives the header via PIO into a local struct and data via DMA directly into the caller's buffer — single round-trip, zero-copy. Client reads larger than 64 KB are split into multiple READ_REQ operations.

### File Write

```
PS2 (client)                     PC (server)
   |                                |
   |-- [WRITE_REQ + WRITE_DATA      |  Combined: WRITE_REQ(12) + WRITE_DATA(8) as
   |    chunk 0] ------------------>|  app header, chunk 0 data as payload
   |<--------- UDPRDMA ACK ---------|
   |                                |
   |-- WRITE_DATA [chunk 1] ------->|  chunk_nr=1
   |<--------- UDPRDMA ACK ---------|
   |     ...                        |
   |-- WRITE_DATA [chunk T-1] ----->|  chunk_nr=T-1 (last)
   |                                |
   |<--------- WRITE_DONE ----------|  result=bytes_written
```

The first packet combines WRITE_REQ (12 bytes) and first WRITE_DATA header (8 bytes) as a 20-byte UDPRDMA app header, with chunk 0 data as the data payload. This saves one round-trip compared to sending WRITE_REQ separately. Subsequent chunks are sent as individual UDPRDMA packets (separately ACK'd). The server accumulates all chunks, writes to file, and responds with WRITE_DONE.

Max data per WRITE_DATA chunk = 1400 - 8 (header) = 1392 bytes.

### Block Read (BREAD)

```
PS2 (client)                     PC (server)
   |                                |
   |-- BREAD_REQ ------------------>|  handle=0, sector, count
   |   (16 bytes)                   |
   |<--------- UDPRDMA ACK ---------|  immediate ACK (request received)
   |                                |  [server reads sectors]
   |<--- [RESULT_REPLY + data       |  RESULT_REPLY(8) as app header,
   |      chunk 0] -----------------|  raw sector data as payload
   |<--- raw data [chunk 1] --------|  UDPRDMA multi-packet
   |     ...                        |
   |<--- raw data [chunk K, FIN] ---|
   |                                |
   |-- UDPRDMA ACK ---------------->|
```

Same pattern as file read: RESULT_REPLY (8 bytes, containing bytes_read) as UDPRDMA app header on the first chunk, raw sector data DMA'd to caller's buffer. Single round-trip, zero-copy.

Large reads (> 128 sectors / 64 KB) are split into multiple BREAD_REQ operations by the client.

### Block Write (BWRITE)

```
PS2 (client)                     PC (server)
   |                                |
   |-- [BWRITE_REQ + WRITE_DATA     |  Combined: BWRITE_REQ(16) + WRITE_DATA(8) as
   |    chunk 0] ------------------>|  app header, chunk 0 data as payload
   |<--------- UDPRDMA ACK ---------|
   |                                |
   |-- WRITE_DATA [chunk 1] ------->|  chunk_nr=1
   |<--------- UDPRDMA ACK ---------|
   |     ...                        |
   |-- WRITE_DATA [chunk T-1] ----->|  chunk_nr=T-1
   |                                |
   |<--------- WRITE_DONE ----------|  result=0
```

### Directory Listing

```
PS2 (client)                     PC (server)
   |                                |
   |-- OPEN_REQ (is_dir=1) -------->|  path
   |<--------- OPEN_REPLY ----------|  handle
   |                                |
   |-- DREAD_REQ ------------------>|  handle
   |<--------- DREAD_REPLY ---------|  entry (name, stat)
   |                                |
   |-- DREAD_REQ ------------------>|  handle
   |<--------- DREAD_REPLY ---------|  entry (name, stat)
   |     ...                        |
   |-- DREAD_REQ ------------------>|  handle
   |<--------- DREAD_REPLY ---------|  result=0 (end of dir)
   |                                |
   |-- CLOSE_REQ ------------------>|  handle
   |<--------- CLOSE_REPLY ---------|
```

### Getstat

```
PS2 (client)                     PC (server)
   |                                |
   |-- GETSTAT_REQ ---------------->|  path
   |<--------- GETSTAT_REPLY -------|  result, stat
```

## Read Chunking Strategy

For raw data responses (file reads and block reads), the server optimizes chunk sizes for DMA efficiency at the PS2 receiver. See [UDPRDMA.md](UDPRDMA.md) Sender Chunk Size Optimization for details.

| Max chunk | Alignment | DMA block size at receiver |
|-----------|-----------|----------------------------|
| 1024      | 512 bytes | 512 bytes                  |
| 1280      | 256 bytes | 256 bytes                  |
| 1408      | 128 bytes | 128 bytes                  |

The server picks the candidate with the fewest packets, breaking ties by largest alignment.

## Flow Control

Large multi-packet transfers (read responses, block reads) use UDPRDMA flow control to prevent the sender from overrunning the receiver. See [UDPRDMA.md](UDPRDMA.md) Flow Control for details.

- **Receiver**: PS2 sends a mid-transfer ACK every 6 packets (`UDPRDMA_RX_ACK_WINDOW = 6`)
- **Sender**: Server waits for ACK before sending more than 8 packets ahead (`SEND_WINDOW = 8`)
- **Window retry limit**: Server retries waiting for window ACK up to 4 times (`MAX_WINDOW_RETRIES = 4`, 100ms per retry). If no ACK is received after 400ms, the transfer is aborted. This prevents the server from blocking indefinitely on flow control, which could cause subsequent requests to time out.

This keeps the server from flooding the PS2 with packets it cannot process fast enough, while still allowing multiple packets in flight for throughput.

### Immediate ACK

The server sends an immediate UDPRDMA ACK upon receiving any request packet, before processing the request. This decouples "request received" from "response ready":

- The PS2 client's `udprdma_send()` returns as soon as the ACK is received (~1ms)
- The client then enters `udprdma_recv()` with a 5-second timeout while the server processes the request
- Without immediate ACK, slow server processing (e.g., disk I/O, flow control delays from previous transfer) could cause the client's send to time out (4 retries x 500ms = 2 seconds)

## Error Handling

### Timeout

| Operation | Timeout | Description |
|-----------|---------|-------------|
| Discovery | 5000ms | UDPRDMA discovery timeout |
| Request/Reply | 5000ms | Default send/recv timeout |
| Write Completion | 5000ms | WRITE_DONE response |
| Block Write | 2000ms | WRITE_DONE for BWRITE (UDPBD only) |

### Error Codes

Negative results in reply messages indicate errors using negated POSIX errno values:

| Error | Value | Description |
|-------|-------|-------------|
| -ENOENT | -2 | File not found |
| -EIO | -5 | I/O error |
| -EBADF | -9 | Bad file handle |
| -EACCES | -13 | Permission denied (read-only, path traversal) |
| -EMFILE | -24 | Too many open files |
| -ENAMETOOLONG | -36 | Path exceeds 256 characters |

## Constants

| Constant | Value | Description |
|----------|-------|-------------|
| UDPFS_SERVICE_ID | 0xF5F5 | UDPRDMA service ID for discovery |
| UDPFS_PORT | 0xF5F6 | UDP port |
| UDPFS_MAX_PATH | 256 | Max path length including null terminator |
| UDPFS_MAX_HANDLES | 8 | Max simultaneous open handles (client) |
| UDPFS_MAX_READ | 65536 | Max bytes per file read request (64 KB) |
| UDPFS_MAX_SECTOR_READ | 128 | Max sectors per block read (64 KB) |
| UDPFS_MAX_PAYLOAD | 1400 | Max UDPFS message size (bytes) |
| BLOCK_DEVICE_HANDLE | 0 | Pre-opened block device handle |

## Constraints

- **Alignment**: All message structures are 4-byte aligned
- **Paths**: Null-terminated, padded to 4-byte boundary, max 256 bytes
- **File addressing**: 64-bit offsets via lo/hi 32-bit words
- **Sector addressing**: 64-bit sector numbers via lo/hi 32-bit words
- **Block read**: Max 128 sectors (64 KB) per BREAD_REQ; client splits larger reads
- **File read**: Max 64 KB per READ_REQ; client loops for larger reads
- **Write chunking**: Max 1392 data bytes per WRITE_DATA chunk (1400 - 8 byte header)
- **Transport**: All messages carried over UDPRDMA on UDP port 0xF5F6 (62966)
- **Read responses**: RESULT_REPLY app header + raw data (zero-copy DMA to caller buffer)
- **Write data**: Each chunk is an individual UDPRDMA packet (separately ACK'd)

## IOP Module Variants

| Module | Description |
|--------|-------------|
| udpfs_ioman.irx | Full UDPFS: registers iomanX "udpfs:" device with file operations |
| udpfs_fhi.irx | FHI variant: uses pre-opened handles for game boot via BREAD/BWRITE |
| udpfs_bd.irx | Block device only: uses BREAD/BWRITE/GETSTAT subset, registers BDM "udp" device |

### iomanX Interface (udpfs_ioman.irx)

Registers as `"udpfs:"` device with `IOP_DT_FSEXT | IOP_DT_FS` type. Supports:

| Operation | Implementation |
|-----------|----------------|
| open | OPEN_REQ/REPLY |
| close | CLOSE_REQ/REPLY |
| read | READ_REQ + raw data |
| write | WRITE_REQ + WRITE_DATA + WRITE_DONE |
| lseek / lseek64 | LSEEK_REQ/REPLY |
| remove | REMOVE_REQ + RESULT_REPLY |
| mkdir | MKDIR_REQ + RESULT_REPLY |
| rmdir | RMDIR_REQ + RESULT_REPLY |
| dopen | OPEN_REQ (is_dir=1) |
| dclose | CLOSE_REQ |
| dread | DREAD_REQ/REPLY |
| getstat | GETSTAT_REQ/REPLY |
| ioctl2(0x80) | Returns server handle for fd |

Unsupported: format, ioctl, chstat, rename, chdir, sync, mount, umount, devctl, symlink, readlink.

### Block Device Interface (udpfs_bd.irx)

Registers with BDM as device name `"udp"`:

```c
int udpbd_init(void);
```

After `udpbd_init()`, the device is available through standard BDM read/write/flush/stop operations using the BREAD/BWRITE/INFO subset with handle 0.

## PC Server

UDPFS is served by `udpfs_server.py`:

```sh
# Block device only (UDPBD mode)
python udpfs_server.py --block-device disk.iso

# Filesystem only
python udpfs_server.py --root-dir /path/to/serve

# Both block device and filesystem
python udpfs_server.py --block-device disk.iso --root-dir /path/to/serve
```

The server opens the block device image as handle 0 and serves files from the root directory. Path traversal outside root_dir is denied.
