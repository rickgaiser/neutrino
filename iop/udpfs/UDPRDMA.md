# UDPRDMA Protocol Specification

**UDP Remote DMA** - A lightweight reliable transport protocol designed for the PS2 IOP. Provides service discovery and reliable data transfer over UDP with DMA-optimized packet structures.

## Design Goals

- **Simple**: Minimal state machine, easy to implement on constrained hardware
- **Reliable**: Go-Back-N ARQ with ACK at completion, immediate NACK on gap
- **DMA-friendly**: 4-byte aligned structures, receiver-selected block sizes
- **Flexible**: Variable length transfers up to 1 MiB (half of IOP RAM)

## Protocol Stack

```
┌─────────────────────────────────┐
│  Application (UDPBD, UDPFS)     │
├─────────────────────────────────┤
│  UDPRDMA (this protocol)        │
├─────────────────────────────────┤
│  UDP                            │
├─────────────────────────────────┤
│  IP                             │
├─────────────────────────────────┤
│  Ethernet                       │
└─────────────────────────────────┘
```

## Packet Types

| Type | Value | Description |
|------|-------|-------------|
| DISCOVERY | 0 | Broadcast to find peers with matching service |
| INFORM | 1 | Response announcing service availability |
| DATA | 2 | Reliable data transfer with ACK/NACK |

## Header Structures

### Alignment Design

The 2-byte base header is intentionally sized to ensure all subsequent data is 4-byte aligned for DMA:

```
Offset  Size  Field                    Alignment
0x00    14    Ethernet header
0x0E    20    IP header
0x22     8    UDP header
0x2A     2    UDPRDMA base header      <- 2 bytes
0x2C     *    Everything after         <- 4-byte aligned (DMA ready)
```

This means Discovery/Inform headers, Data headers, and all payload data can be efficiently DMA transferred.

### Base Header (2 bytes)

Present in all UDPRDMA packets. Positioned to align all following data on 4-byte boundary.

```
 0                   1
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| type  |       seq_nr          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| Field | Bits | Description |
|-------|------|-------------|
| type | 4 | Packet type (0=DISCOVERY, 1=INFORM, 2=DATA) |
| seq_nr | 12 | Sequence number (0-4095) |

### Discovery/Inform Header (4 bytes after base)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          service_id           |           reserved            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| Field | Bits | Description |
|-------|------|-------------|
| service_id | 16 | Service identifier (e.g., 0xF5F5 for UDPFS) |
| reserved | 16 | Reserved (must be 0) |

### Data Header (4 bytes after base)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       seq_nr_ack      |flags|h_wc|       data_byte_count       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| Field | Bits | Description |
|-------|------|-------------|
| seq_nr_ack | 12 | ACK: last received seq_nr; NACK: expected seq_nr |
| flags | 2 | See flags table below |
| hdr_word_count | 4 | App header size in 4-byte words (0 = no header, max 15 = 60 bytes) |
| data_byte_count | 14 | Data payload size in bytes (0 = ACK/NACK only, max 16383) |

**Flags:**

| Flag | Value | Description |
|------|-------|-------------|
| ACK | 0x1 | 1=ACK (seq_nr_ack is last received), 0=NACK (seq_nr_ack is expected) |
| FIN | 0x2 | Final packet of transfer |

**Payload layout**: When `hdr_word_count > 0`, the payload is split into two parts:
```
[app_header: hdr_word_count × 4 bytes] [data: data_byte_count bytes]
```
The app header is delivered to the receiver via PIO (register reads) into a separate buffer, while data is DMA'd to the main receive buffer. When `hdr_word_count == 0`, the entire payload is data (backward compatible).

**Max payload**: 1408 bytes per packet (fixed, = 11 × 128). Total payload = app_header + data. Data must be a multiple of 4 bytes; sender pads if needed. Max app header: 32 bytes (UDPRDMA_MAX_APP_HDR).

**DMA block size**: The receiver selects the largest DMA block size (512, 256, or 128 bytes) that evenly divides `data_byte_count`. If none fit exactly, the 128-byte aligned portion is DMA'd and the remainder is PIO'd. Falls back to 64-byte DMA blocks for data < 128 bytes, and pure PIO for data < 64 bytes. Older SPEED chips (rev <= 0x12) limit DMA block size to 128 bytes.

## Service Discovery

### Client Discovery Flow

1. Client broadcasts DISCOVERY with desired service_id
2. Server responds with INFORM if it provides that service
3. Client extracts server IP from INFORM packet
4. Connection established

```
Client                           Server
   |                                |
   |-- DISCOVERY (broadcast) ------>|
   |   service_id=0xF5F5            |
   |                                |
   |<--------- INFORM --------------|
   |   service_id=0xF5F5            |
   |                                |
   [Connection established]
```

### Sequence Number Initialization

After discovery/inform exchange:
- Both sides start the data connecton with sequence number 0 and use `rx_seq_nr_expected = (peer's seq_nr + 1) & 0xFFF` for the next packet

## Reliable Data Transfer

### Go-Back-N ARQ

UDPRDMA uses Go-Back-N with:
- **ACK at completion**: Single ACK after all data received
- **Immediate NACK on gap**: As soon as out-of-order packet detected

### Sender Behavior

1. Send packets with incrementing seq_nr
2. Set FIN flag on final packet
3. Wait for ACK or NACK
4. On NACK: retransmit from indicated seq_nr
5. On timeout: retransmit from last ACK'd seq_nr

### Receiver Behavior

1. Expect packets in order (matching seq_nr_expected)
2. **In-order packet**: DMA to buffer, increment seq_nr_expected
3. **Out-of-order packet**: Discard, send NACK immediately
4. **FIN packet received**: Send ACK

### ACK/NACK Encoding

Both use DATA packet with hdr_word_count=0 and data_byte_count=0:

**ACK (flags.ACK=1):**
- seq_nr_ack = last received seq_nr
- Sent after receiving FIN packet

**NACK (flags.ACK=0):**
- seq_nr_ack = expected seq_nr (the one that's missing)
- Sent immediately when out-of-order detected

### Piggybacked ACK

DATA packets carrying payload include ACK information for the reverse direction:
```
seq_nr = sender's sequence number
seq_nr_ack = cumulative ACK for peer's data
flags.ACK = 1
data_byte_count > 0
```

## Variable Length Transfers

UDPRDMA handles transfers of any size from 1 byte to 1 MiB:

### Small Transfer (fits in one packet)

```
Sender                          Receiver
   |                                |
   |-- DATA [payload, FIN] -------->|  seq=N
   |                                |
   |<--------- DATA [ACK] ----------|  seq_nr_ack=N
```

### Large Transfer (multiple packets)

```
Sender                          Receiver
   |                                |
   |-- DATA [chunk 0] ------------->|  seq=N
   |-- DATA [chunk 1] ------------->|  seq=N+1
   |-- DATA [chunk 2] ------------->|  seq=N+2
   |   ...                          |
   |-- DATA [chunk K, FIN] -------->|  seq=N+K
   |                                |
   |<--------- DATA [ACK] ----------|  seq_nr_ack=N+K
```

### Transfer Size Limits

| Limit | Value | Reason |
|-------|-------|--------|
| Min | 1 byte | Minimum useful transfer |
| Max | 1 MiB | Half of IOP RAM (2 MiB total) |
| Chunk | ~1400 bytes | UDP payload - headers |

### Sender Chunk Size Optimization

For multi-packet transfers, the sender chooses chunk size to minimize total packets while maximizing DMA efficiency at the receiver:

| Max chunk | Alignment | DMA block size |
|-----------|-----------|----------------|
| 1024      | 512 bytes | 512 bytes      |
| 1280      | 256 bytes | 256 bytes      |
| 1408      | 128 bytes | 128 bytes      |

Algorithm: For N total bytes, pick the candidate with fewest `ceil(N/max_chunk)` packets. Break ties by largest alignment (best DMA efficiency).

### Receive Accumulation

The receiver accumulates data sequentially into the receive buffer. Each in-order packet appends its data at the current write position:
```c
dest = buffer + rx_received
rx_received += data_byte_count
```
`rx_received` tracks how many bytes have been written so far. The transfer is complete when FIN is received and ACK is sent.

## Flow Control

For large multi-packet transfers, the receiver sends mid-transfer ACKs to prevent the sender from overrunning the receive buffer:

- **Receiver**: Sends ACK every `RX_ACK_WINDOW` (6) packets, using the normal ACK encoding (flags.ACK=1, seq_nr_ack = last received seq_nr). These are non-FIN ACKs — the sender must not treat them as transfer completion.
- **Sender**: Tracks unacknowledged packets and pauses when `SEND_WINDOW` (8) packets are outstanding. Resumes on receiving the mid-transfer ACK.

```
Sender                          Receiver
   |                                |
   |-- DATA [chunk 0] ------------->|  seq=N
   |-- DATA [chunk 1] ------------->|  seq=N+1
   |   ...                          |
   |-- DATA [chunk 5] ------------->|  seq=N+5
   |                                |
   |<--------- DATA [ACK] ----------|  seq_nr_ack=N+5 (window ACK)
   |                                |
   |-- DATA [chunk 6] ------------->|  seq=N+6
   |   ...                          |
   |-- DATA [chunk K, FIN] -------->|  seq=N+K
   |                                |
   |<--------- DATA [ACK] ----------|  seq_nr_ack=N+K (final ACK)
```

The window ACK is distinguished from a final ACK by the absence of FIN — the sender knows the transfer is complete only when it has sent FIN and receives an ACK for that packet.

## Packet Loss Recovery

```
Sender                          Receiver
   |                                |
   |-- DATA [chunk 0] ------------->|  seq=N
   |-- DATA [chunk 1] ------------->|  seq=N+1
   |-- [chunk 2 LOST] ----X         |
   |-- DATA [chunk 3] ------------->|  seq=N+3 (out of order!)
   |                                |
   |<--------- DATA [NACK] ---------|  seq_nr_ack=N+2 (expected)
   |                                |
   |-- DATA [chunk 2] ------------->|  seq=N+2 (retransmit)
   |-- DATA [chunk 3] ------------->|  seq=N+3
   |-- DATA [chunk 4] ------------->|  seq=N+4
   |   ...                          |
   |-- DATA [chunk K, FIN] -------->|  seq=N+K
   |                                |
   |<--------- DATA [ACK] ----------|  seq_nr_ack=N+K
```

## Timing Constants

| Parameter | Value | Description |
|-----------|-------|-------------|
| RETX_TIMEOUT | 500ms | Retransmit if no ACK/NACK received |
| DISC_TIMEOUT | 2000ms | Discovery timeout |
| MAX_RETRIES | 4 | Max retransmit attempts before disconnect |

## Error Codes

| Code | Value | Description |
|------|-------|-------------|
| UDPRDMA_OK | 0 | Success |
| UDPRDMA_ERR_TIMEOUT | -1 | Operation timed out; sets STATE_DISCONNECTED |
| UDPRDMA_ERR_NACK | -2 | Too many retransmits |
| UDPRDMA_ERR_NOSOCKET | -3 | No socket available |
| UDPRDMA_ERR_INVAL | -4 | Invalid argument |
| UDPRDMA_ERR_NOTCONN | -5 | Not connected |
| UDPRDMA_ERR_NOBUF | -6 | No buffer available |

## API

```c
// Create socket with service ID
udprdma_socket_t *udprdma_create(uint16_t port, uint16_t service_id);

// Destroy socket
void udprdma_destroy(udprdma_socket_t *socket);

// Discover peer (client mode)
int udprdma_discover(udprdma_socket_t *socket, uint32_t timeout_ms);

// Send INFORM message (server mode)
void udprdma_inform(udprdma_socket_t *socket);

// Send data reliably (single packet, max 1408 bytes)
int udprdma_send(udprdma_socket_t *socket, const void *data, uint32_t size);

// Send with scatter-gather: app_hdr packed in packet, data sent via DMA
// app_hdr_size must be multiple of 4, max UDPRDMA_MAX_APP_HDR (32) bytes
int udprdma_send_ll(udprdma_socket_t *socket,
                    const void *app_hdr, uint32_t app_hdr_size,
                    const void *data, uint32_t data_size);

// Set receive buffer for DMA (call before recv for multi-packet transfers)
void udprdma_set_rx_buffer(udprdma_socket_t *socket, void *buffer, uint32_t size);

// Set receive app header buffer (PIO'd separately from DMA data)
// Reset automatically after udprdma_recv() completes
void udprdma_set_rx_app_header(udprdma_socket_t *socket, void *hdr_buf, uint32_t hdr_size);

// Receive data reliably (supports multi-packet, up to buffer size)
int udprdma_recv(udprdma_socket_t *socket, void *buffer, uint32_t size, uint32_t timeout_ms);

// Check connection state
int udprdma_is_connected(udprdma_socket_t *socket);

// Get peer IP address
uint32_t udprdma_get_peer_ip(udprdma_socket_t *socket);
```

## Constraints

- **Alignment**: All buffers must be 4-byte aligned
- **Header sizes**: All headers after base are 4-byte multiples
- **Payload sizes**: Always multiple of 4 bytes (padded if needed)
- **DMA block size**: Receiver selects from 512/256/128/64 bytes; max 128 bytes on older SPEED chips (rev <= 0x12)
- **DMA efficiency**: DMA used for >= 64 bytes; smaller uses PIO word copy
- **Max payload**: 1408 bytes per packet (fixed)
- **Max transfer**: 1 MiB per send/recv call
- **Sequence space**: 12-bit (4096), wraps for transfers > ~5 MiB
- **Socket limit**: Max 2 UDPRDMA sockets

## Service IDs

| Service | ID | Description |
|---------|------|-------------|
| UDPFS | 0xF5F5 | File system and block device access |

## Default Port

UDPRDMA uses UDP port **0xF5F6 (62966)** by default.
