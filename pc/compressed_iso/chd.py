"""
CHD (Compressed Hunks of Data) format wrapper.

Supports CHD v5 format used by MAME and PCSX2.
Implements zlib and lzma decompression (most common for PS2 ISOs).

CHD uses "hunks" which are similar to blocks in CSO/ZSO.
Each hunk can be compressed with different codecs.

Based on MAME chd.cpp/chdcodec.cpp source code.
"""

import struct
import sys
import zlib
import lzma
from typing import List, Optional, Tuple

from .base import CompressedFileWrapper


# CHD magic and version
CHD_MAGIC = b'MComprHD'
CHD_VERSION = 5

# Compression codec IDs (4-char codes stored as big-endian uint32)
CHD_CODEC_NONE = 0
CHD_CODEC_CDLZ = 0x63646c7a  # "cdlz" - CD LZMA
CHD_CODEC_CDZL = 0x63647a6c  # "cdzl" - CD Deflate (zlib)
CHD_CODEC_CDFL = 0x6364666c  # "cdfl" - CD FLAC
CHD_CODEC_ZLIB = 0x7a6c6962  # "zlib"
CHD_CODEC_ZLIB_PLUS = 0x7a6c702b  # "zlp+"
CHD_CODEC_LZMA = 0x6c7a6d61  # "lzma"
CHD_CODEC_HUFF = 0x68756666  # "huff"
CHD_CODEC_FLAC = 0x666c6163  # "flac"

# CD sector constants (from cdrom.h)
CD_MAX_SECTOR_DATA = 2352
CD_MAX_SUBCODE_DATA = 96
CD_FRAME_SIZE = CD_MAX_SECTOR_DATA + CD_MAX_SUBCODE_DATA  # 2448

# CD sync header for ECC detection
CD_SYNC_HEADER = bytes([0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00])

# V5 map compression types (from chd.cpp)
COMPRESSION_TYPE_0 = 0  # codec #0
COMPRESSION_TYPE_1 = 1  # codec #1
COMPRESSION_TYPE_2 = 2  # codec #2
COMPRESSION_TYPE_3 = 3  # codec #3
COMPRESSION_NONE = 4    # no compression
COMPRESSION_SELF = 5    # self-reference
COMPRESSION_PARENT = 6  # parent-reference

# Pseudo-types for RLE encoding
COMPRESSION_RLE_SMALL = 7
COMPRESSION_RLE_LARGE = 8
COMPRESSION_SELF_0 = 9
COMPRESSION_SELF_1 = 10
COMPRESSION_PARENT_SELF = 11
COMPRESSION_PARENT_0 = 12
COMPRESSION_PARENT_1 = 13


class BitstreamReader:
    """Read bits from a byte stream, MSB first (matches MAME bitstream_in)."""

    def __init__(self, data: bytes):
        self.data = data
        self.offset = 0  # current bit position

    def read(self, num_bits: int) -> int:
        """Consume and return num_bits, MSB first."""
        if num_bits == 0:
            return 0
        result = 0
        bits_read = 0
        while bits_read < num_bits:
            byte_offset = self.offset // 8
            bit_offset = self.offset % 8
            if byte_offset >= len(self.data):
                raise ValueError("Bitstream overflow")
            bits_available = 8 - bit_offset
            bits_needed = num_bits - bits_read
            bits_to_read = min(bits_available, bits_needed)
            byte_val = self.data[byte_offset]
            shift = bits_available - bits_to_read
            bits = (byte_val >> shift) & ((1 << bits_to_read) - 1)
            result = (result << bits_to_read) | bits
            bits_read += bits_to_read
            self.offset += bits_to_read
        return result

    def peek(self, num_bits: int) -> int:
        """Return num_bits without consuming; zero-pads at EOF (like MAME buffer)."""
        if num_bits == 0:
            return 0
        result = 0
        bits_read = 0
        pos = self.offset
        while bits_read < num_bits:
            byte_offset = pos // 8
            if byte_offset >= len(self.data):
                result <<= (num_bits - bits_read)  # zero-pad
                break
            bit_offset = pos % 8
            bits_available = 8 - bit_offset
            bits_needed = num_bits - bits_read
            bits_to_read = min(bits_available, bits_needed)
            byte_val = self.data[byte_offset]
            shift = bits_available - bits_to_read
            bits = (byte_val >> shift) & ((1 << bits_to_read) - 1)
            result = (result << bits_to_read) | bits
            bits_read += bits_to_read
            pos += bits_to_read
        return result

    def remaining(self) -> int:
        return len(self.data) * 8 - self.offset


class HuffmanDecoder:
    """Huffman decoder matching MAME huffman_context_base / huffman_decoder<N,M>.

    Supports two tree-import modes:
      import_tree_rle()     — used by CHD v5 map (huffman_decoder<16,8>)
      import_tree_huffman() — used by huff data codec (huffman_decoder<256,16>)
    """

    def __init__(self, maxbits: int = 16, numcodes: int = 256):
        self.maxbits = maxbits
        self.numcodes = numcodes
        # RLE numbits: 5 if maxbits>=16, 4 if >=8, else 3
        self.numbits = 5 if maxbits >= 16 else (4 if maxbits >= 8 else 3)
        self.huffnode = [{'numbits': 0, 'code': 0} for _ in range(numcodes)]
        # lookup_array[peek(maxbits)] = (symbol << 5) | numbits  (MAKE_LOOKUP)
        self._lookup_array = [0] * (1 << maxbits)

    def import_tree_rle(self, bitstream: BitstreamReader) -> None:
        """Import tree bit-lengths via simple RLE (MAME import_tree_rle).

        Used for CHD v5 compressed map decoding.
        """
        curnode = 0
        while curnode < self.numcodes:
            nodebits = bitstream.read(self.numbits)
            if nodebits != 1:
                self.huffnode[curnode]['numbits'] = nodebits
                curnode += 1
            else:
                nodebits2 = bitstream.read(self.numbits)
                if nodebits2 == 1:
                    self.huffnode[curnode]['numbits'] = 1
                    curnode += 1
                else:
                    repcount = bitstream.read(self.numbits) + 3
                    while repcount > 0 and curnode < self.numcodes:
                        self.huffnode[curnode]['numbits'] = nodebits2
                        curnode += 1
                        repcount -= 1

        if curnode != self.numcodes:
            raise ValueError(f"Huffman RLE tree incomplete: {curnode}/{self.numcodes}")

        self._assign_canonical_codes()
        self._build_lookup_table()

    def import_tree_huffman(self, bitstream: BitstreamReader) -> None:
        """Import tree encoded with a secondary huffman_decoder<24,6> (MAME import_tree_huffman).

        Used by huffman_8bit_encoder::encode() → export_tree_huffman().
        This is the correct mode for the CHD 'huff' data codec.
        """
        # --- Parse the small tree (huffman_decoder<24,6>) ---
        small = HuffmanDecoder(maxbits=6, numcodes=24)
        small.huffnode[0]['numbits'] = bitstream.read(3)
        start = bitstream.read(3) + 1  # first_non_zero
        count = 0
        for index in range(1, 24):
            if index < start or count == 7:
                small.huffnode[index]['numbits'] = 0
            else:
                count = bitstream.read(3)
                small.huffnode[index]['numbits'] = 0 if count == 7 else count
        small._assign_canonical_codes()
        small._build_lookup_table()

        # --- Compute rlefullbits = floor(log2(numcodes - 9)) ---
        rlefullbits = 0
        temp = self.numcodes - 9
        while temp != 0:
            temp >>= 1
            rlefullbits += 1

        # --- Decode main tree bit-lengths via small tree ---
        last = 0
        curcode = 0
        while curcode < self.numcodes:
            value = small.decode_one(bitstream)
            if value != 0:
                self.huffnode[curcode]['numbits'] = value - 1
                last = value - 1
                curcode += 1
            else:
                # RLE token: read 3-bit count (+2); if raw==7 read rlefullbits more
                rle_count = bitstream.read(3) + 2
                if rle_count == 9:  # raw was 7 → 7+2=9
                    rle_count += bitstream.read(rlefullbits)
                while rle_count > 0 and curcode < self.numcodes:
                    self.huffnode[curcode]['numbits'] = last
                    curcode += 1
                    rle_count -= 1

        if curcode != self.numcodes:
            raise ValueError(f"Huffman tree incomplete: {curcode}/{self.numcodes}")

        self._assign_canonical_codes()
        self._build_lookup_table()

    def _assign_canonical_codes(self) -> None:
        """Assign canonical codes, matching MAME assign_canonical_codes() exactly.

        Uses bithisto[33] loop from codelen=32 down to 1, then assigns codes in
        node order (same as MAME: node.m_bits = bithisto[node.m_numbits]++).
        """
        bithisto = [0] * 33
        for node in self.huffnode:
            bits = node['numbits']
            if 0 < bits <= 32:
                bithisto[bits] += 1

        curstart = 0
        for codelen in range(32, 0, -1):
            nextstart = (curstart + bithisto[codelen]) >> 1
            bithisto[codelen] = curstart   # repurpose as starting code
            curstart = nextstart

        for node in self.huffnode:
            bits = node['numbits']
            if bits > 0:
                node['code'] = bithisto[bits]
                bithisto[bits] += 1

    def _build_lookup_table(self) -> None:
        """Build O(1) lookup array matching MAME build_lookup_table().

        For each code: fill all lookup_array entries whose top numbits match
        the code. Entry value = MAKE_LOOKUP(symbol, numbits) = (symbol<<5)|numbits.
        """
        self._lookup_array = [0] * (1 << self.maxbits)
        for i, node in enumerate(self.huffnode):
            bits = node['numbits']
            if 0 < bits <= self.maxbits:
                value = (i << 5) | (bits & 0x1f)
                shift = self.maxbits - bits
                start = node['code'] << shift
                end = (node['code'] + 1) << shift
                for j in range(start, end):
                    self._lookup_array[j] = value

    def decode_one(self, bitstream: BitstreamReader) -> int:
        """Decode one symbol using O(1) table lookup (MAME decode_one).

        peek(maxbits) → lookup_array[bits] → remove(numbits) → return symbol.
        """
        bits = bitstream.peek(self.maxbits)
        lookup = self._lookup_array[bits]
        if lookup == 0:
            # Unrecognised code — consume 1 bit to avoid infinite loop
            bitstream.read(1)
            return 0
        bitstream.read(lookup & 0x1f)  # consume the actual code bits
        return lookup >> 5


class ChdFileWrapper(CompressedFileWrapper):
    """CHD (Compressed Hunks of Data) format wrapper.
    
    Supports CHD v5 format used by MAME and PCSX2.
    """
    
    def __init__(self, file_path: str, cache_size: int = None):
        """Initialize CHD wrapper."""
        super().__init__(file_path, cache_size)
        self.compressors: List[int] = []
        self.map_offset = 0
        self.hunk_size = 0
        self.total_hunks = 0
        self._map_entries: List[Tuple[int, int, int]] = []  # (offset, complength, compression)
        self._is_cd_format = False  # True if using CD codecs (cdlz/cdzl/cdfl)
        try:
            self._parse_header()
        except Exception as e:
            if self.file:
                self.file.close()
                self.file = None
            raise
    
    def _parse_header(self):
        """Parse CHD v5 header and map."""
        self.file.seek(0)
        header = self.file.read(124)
        
        if len(header) < 124:
            raise ValueError(f"Invalid CHD file: header too short ({len(header)} bytes)")
        
        # Check magic
        magic = header[0:8]
        if magic != CHD_MAGIC:
            raise ValueError(f"Invalid CHD magic: {magic!r}")
        
        # Parse header fields
        header_length = struct.unpack('>I', header[8:12])[0]
        version = struct.unpack('>I', header[12:16])[0]
        
        if version != CHD_VERSION:
            raise ValueError(f"Unsupported CHD version: {version}")
        
        # Compressors (4 x uint32, big-endian)
        self.compressors = list(struct.unpack('>4I', header[16:32]))
        
        # Logical length (uncompressed size)
        self.uncompressed_size = struct.unpack('>Q', header[32:40])[0]
        
        # Map offset
        self.map_offset = struct.unpack('>Q', header[40:48])[0]
        
        # Meta offset
        meta_offset = struct.unpack('>Q', header[48:56])[0]
        
        # Hunk size
        self.hunk_size = struct.unpack('>I', header[56:60])[0]
        self.block_size = self.hunk_size
        
        if self.hunk_size == 0:
            raise ValueError("Invalid CHD: hunk_size is 0")
        
        # Unit size
        unit_size = struct.unpack('>I', header[60:64])[0]
        
        # Calculate total hunks
        self.total_hunks = (self.uncompressed_size + self.hunk_size - 1) // self.hunk_size
        self._num_blocks = self.total_hunks
        
        # Calculate frames per hunk for CD codec
        self._frames_per_hunk = self.hunk_size // CD_FRAME_SIZE

        # Detect CD-format CHD (cdlz/cdzl/cdfl codecs store 2448-byte frames)
        cd_codecs = (CHD_CODEC_CDLZ, CHD_CODEC_CDZL, CHD_CODEC_CDFL)
        self._is_cd_format = any(c in cd_codecs for c in self.compressors if c != 0)

        if self._is_cd_format and self.hunk_size % CD_FRAME_SIZE == 0:
            # CD format: present as 2048-byte/sector ISO by extracting user data
            # Override uncompressed_size (CHD value includes 96-byte subcode per frame)
            total_frames = self.total_hunks * self._frames_per_hunk
            self.uncompressed_size = total_frames * 2048
            self.block_size = self._frames_per_hunk * 2048  # e.g. 8 * 2048 = 16384

        # Read map
        self._read_map()
    
    def _read_map(self):
        """Read and parse the CHD v5 compressed map.

        From chd.cpp decompress_v5_map():
        1. Read 16-byte header: mapbytes, firstoffs, mapcrc, lengthbits, selfbits, parentbits
        2. Read mapbytes of bitstream data
        3. Decode Huffman-encoded compression types
        4. Decode offset/length/crc for each hunk
        """
        # Read map header (16 bytes)
        self.file.seek(self.map_offset)
        rawbuf = self.file.read(16)
        
        if len(rawbuf) < 16:
            raise ValueError("Invalid CHD: map header too short")
        
        # Parse header (big-endian)
        mapbytes = struct.unpack('>I', rawbuf[0:4])[0]
        firstoffs = int.from_bytes(rawbuf[4:10], 'big')  # UINT48
        mapcrc = struct.unpack('>H', rawbuf[10:12])[0]
        lengthbits = rawbuf[12]
        selfbits = rawbuf[13]
        parentbits = rawbuf[14]
        
        # Read compressed map data
        compressed = self.file.read(mapbytes)
        if len(compressed) < mapbytes:
            raise ValueError(f"Invalid CHD: map data truncated")

        # Create bitstream reader
        bitstream = BitstreamReader(compressed)
        
        # Decode Huffman tree for compression types
        # huffman_decoder<16, 8> from chd.cpp means NumCodes=16, MaxBits=8
        # Template params are <_NumCodes, _MaxBits>
        decoder = HuffmanDecoder(maxbits=8, numcodes=16)
        try:
            decoder.import_tree_rle(bitstream)
        except Exception as e:
            raise ValueError(f"CHD: Huffman map decode failed: {e}")
        
        # Decode compression types for all hunks
        compression_types = []
        lastcomp = 0
        repcount = 0
        
        for hunknum in range(self.total_hunks):
            if repcount > 0:
                compression_types.append(lastcomp)
                repcount -= 1
            else:
                val = decoder.decode_one(bitstream)
                if val == COMPRESSION_RLE_SMALL:
                    compression_types.append(lastcomp)
                    repcount = 2 + decoder.decode_one(bitstream)
                elif val == COMPRESSION_RLE_LARGE:
                    compression_types.append(lastcomp)
                    repcount = 2 + 16 + (decoder.decode_one(bitstream) << 4)
                    repcount += decoder.decode_one(bitstream)
                else:
                    compression_types.append(val)
                    lastcomp = val
        
        # Decode offset/length/crc for each hunk
        self._map_entries = []
        curoffset = firstoffs
        last_self = 0
        
        for hunknum in range(self.total_hunks):
            comp_type = compression_types[hunknum]
            offset = curoffset
            length = 0
            crc = 0
            
            if comp_type in (COMPRESSION_TYPE_0, COMPRESSION_TYPE_1, COMPRESSION_TYPE_2, COMPRESSION_TYPE_3):
                length = bitstream.read(lengthbits)
                crc = bitstream.read(16)
                curoffset += length
            elif comp_type == COMPRESSION_NONE:
                length = self.hunk_size
                crc = bitstream.read(16)
                curoffset += length
            elif comp_type == COMPRESSION_SELF:
                offset = bitstream.read(selfbits)
                last_self = offset
            elif comp_type == COMPRESSION_PARENT:
                offset = bitstream.read(parentbits)
            elif comp_type == COMPRESSION_SELF_0:
                offset = last_self
                comp_type = COMPRESSION_SELF
            elif comp_type == COMPRESSION_SELF_1:
                last_self += 1
                offset = last_self
                comp_type = COMPRESSION_SELF
            elif comp_type == COMPRESSION_PARENT_SELF:
                offset = (hunknum * self.hunk_size) // (self.hunk_size // self._frames_per_hunk)
                comp_type = COMPRESSION_PARENT
            else:
                # Unknown type, treat as compressed
                length = bitstream.read(lengthbits)
                crc = bitstream.read(16)
                curoffset += length
            
            self._map_entries.append((offset, length, comp_type))
    
    def _decompress_block(self, block_idx: int) -> bytes:
        """Decompress a single CHD hunk."""
        if block_idx >= len(self._map_entries):
            raise ValueError(f"Hunk index {block_idx} out of range")

        offset, complength, compression = self._map_entries[block_idx]
        
        # Handle different compression types
        if compression == COMPRESSION_NONE:
            # Uncompressed
            self.file.seek(offset)
            data = self.file.read(self.hunk_size)
            if len(data) < self.hunk_size:
                data += b'\x00' * (self.hunk_size - len(data))
            if self._is_cd_format:
                result = bytearray()
                for i in range(self._frames_per_hunk):
                    frame_start = i * CD_FRAME_SIZE
                    sector = data[frame_start:frame_start + CD_MAX_SECTOR_DATA]
                    result.extend(self._extract_user_data_from_sector(sector, False))
                return bytes(result)
            return data
        
        if compression == COMPRESSION_SELF:
            # Self-reference - copy from another hunk
            if offset < len(self._map_entries):
                return self._decompress_block(offset)
            raise ValueError(f"Self-reference to invalid hunk {offset}")
        
        if compression == COMPRESSION_PARENT:
            raise ValueError("Parent CHD not supported")
        
        # Compressed with one of the codecs
        codec_idx = compression  # 0-3 maps to compressors[0-3]
        if codec_idx < 0 or codec_idx > 3:
            codec_idx = 0
        
        codec = self.compressors[codec_idx] if codec_idx < len(self.compressors) else CHD_CODEC_NONE
        
        self.file.seek(offset)
        compressed_data = self.file.read(complength if complength > 0 else self.hunk_size)

        if codec == CHD_CODEC_CDLZ:
            return self._decompress_cd_lzma(compressed_data, block_idx)
        elif codec == CHD_CODEC_CDZL:
            return self._decompress_cd_zlib(compressed_data, block_idx)
        elif codec == CHD_CODEC_LZMA:
            return self._decompress_lzma(compressed_data, block_idx)
        elif codec in (CHD_CODEC_ZLIB, CHD_CODEC_ZLIB_PLUS):
            return self._decompress_zlib(compressed_data, block_idx)
        elif codec == CHD_CODEC_FLAC:
            return self._decompress_flac(compressed_data, block_idx)
        elif codec == CHD_CODEC_HUFF:
            return self._decompress_huff(compressed_data, block_idx)
        else:
            codec_name = f'0x{codec:08x}'
            for name, val in [('cdlz', CHD_CODEC_CDLZ), ('cdzl', CHD_CODEC_CDZL),
                               ('cdfl', CHD_CODEC_CDFL)]:
                if codec == val:
                    codec_name = name
                    break
            print(f"CHD: unsupported codec '{codec_name}' for hunk {block_idx}", file=sys.stderr)
            return b'\x00' * self.block_size
    
    def _extract_user_data_from_sector(self, sector: bytes, ecc_set: bool) -> bytes:
        """Extract 2048 bytes of user data from a 2352-byte raw CD sector.

        For Mode 1 sectors: user data at bytes 16-2063.
        For Mode 2 Form 1 sectors (PS2 DVD): user data at bytes 24-2071.
        For ECC-stripped sectors: sync header is zeroed by MAME, but mode byte
        at offset 15 and user data at standard offsets are intact.
        For sectors without a sync header: treat bytes 0-2047 as user data.
        """
        if len(sector) < 2048:
            return bytes(sector).ljust(2048, b'\x00')

        if sector[0:12] == CD_SYNC_HEADER:
            # Standard CD sector with sync header present
            mode = sector[15] if len(sector) > 15 else 1
            offset = 24 if mode == 2 else 16
        elif ecc_set:
            # ECC stripped by MAME: sync header zeroed, user data at standard offset
            mode = sector[15] if len(sector) > 15 else 1
            offset = 24 if mode == 2 else 16
        else:
            # No recognizable sync header — raw data sector, user data starts at 0
            offset = 0

        end = offset + 2048
        if end > len(sector):
            return bytes(sector[offset:]).ljust(2048, b'\x00')
        return bytes(sector[offset:end])

    def _decompress_cd_lzma(self, compressed_data: bytes, block_idx: int) -> bytes:
        """Decompress CD LZMA format."""
        frames = self._frames_per_hunk
        complen_bytes = 2 if self.hunk_size < 65536 else 3
        ecc_bytes = (frames + 7) // 8
        header_bytes = ecc_bytes + complen_bytes

        if len(compressed_data) < header_bytes:
            return bytes(self.block_size)

        ecc_flags = compressed_data[:ecc_bytes]

        if complen_bytes == 2:
            complen_base = (compressed_data[ecc_bytes] << 8) | compressed_data[ecc_bytes + 1]
        else:
            complen_base = (compressed_data[ecc_bytes] << 16) | (compressed_data[ecc_bytes + 1] << 8) | compressed_data[ecc_bytes + 2]

        base_data = compressed_data[header_bytes:header_bytes + complen_base]

        # Try raw LZMA1 with MAME's fixed properties first
        sector_data = None
        try:
            lzma_filter = {'id': lzma.FILTER_LZMA1, 'lc': 3, 'lp': 0, 'pb': 2,
                           'dict_size': self.hunk_size}
            sector_data = lzma.LZMADecompressor(
                format=lzma.FORMAT_RAW, filters=[lzma_filter]).decompress(base_data)
        except lzma.LZMAError:
            pass

        # Fallback: standard LZMA with 5-byte header
        if sector_data is None:
            try:
                sector_data = lzma.decompress(base_data)
            except lzma.LZMAError:
                pass

        # Fallback: raw LZMA1 with properties encoded in first byte
        if sector_data is None and len(base_data) >= 5:
            try:
                props = base_data[0]
                lc = props % 9
                lp = (props // 9) % 5
                pb = props // 45
                dict_size = struct.unpack('<I', base_data[1:5])[0]
                lzma_filter = {'id': lzma.FILTER_LZMA1, 'lc': lc, 'lp': lp, 'pb': pb,
                               'dict_size': dict_size if dict_size > 0 else self.hunk_size}
                sector_data = lzma.LZMADecompressor(
                    format=lzma.FORMAT_RAW, filters=[lzma_filter]).decompress(base_data[5:])
            except (lzma.LZMAError, struct.error):
                pass

        if sector_data is None:
            return bytes(self.block_size)

        result = bytearray()
        for framenum in range(frames):
            sector_start = framenum * CD_MAX_SECTOR_DATA
            sector = sector_data[sector_start:sector_start + CD_MAX_SECTOR_DATA]
            ecc_set = (ecc_flags[framenum // 8] & (1 << (framenum % 8))) != 0
            result.extend(self._extract_user_data_from_sector(sector, ecc_set))
        return bytes(result)
    
    def _decompress_cd_zlib(self, compressed_data: bytes, block_idx: int) -> bytes:
        """Decompress CD zlib format."""
        frames = self._frames_per_hunk
        destlen = self.hunk_size
        complen_bytes = 2 if destlen < 65536 else 3
        ecc_bytes = (frames + 7) // 8
        header_bytes = ecc_bytes + complen_bytes
        
        if len(compressed_data) < header_bytes:
            return bytes(self.block_size)

        ecc_flags = compressed_data[:ecc_bytes]
        
        if complen_bytes == 2:
            complen_base = (compressed_data[ecc_bytes] << 8) | compressed_data[ecc_bytes + 1]
        else:
            complen_base = (compressed_data[ecc_bytes] << 16) | (compressed_data[ecc_bytes + 1] << 8) | compressed_data[ecc_bytes + 2]
        
        base_data = compressed_data[header_bytes:header_bytes + complen_base]

        try:
            base_decompressor = zlib.decompressobj(-zlib.MAX_WBITS)
            sector_data = base_decompressor.decompress(base_data)
        except zlib.error:
            return bytes(self.block_size)

        # Extract 2048-byte user data from each sector (discard subcode)
        result = bytearray()
        for framenum in range(frames):
            sector_start = framenum * CD_MAX_SECTOR_DATA
            sector = sector_data[sector_start:sector_start + CD_MAX_SECTOR_DATA]
            ecc_set = (ecc_flags[framenum // 8] & (1 << (framenum % 8))) != 0
            result.extend(self._extract_user_data_from_sector(sector, ecc_set))

        return bytes(result)
    
    def _decompress_lzma(self, compressed_data: bytes, block_idx: int) -> bytes:
        """Decompress standard LZMA format (raw LZMA1 as used by MAME CHD)."""
        data = None

        # Try raw LZMA1 with MAME's fixed properties (from chdcodec.cpp s_lzma_props)
        # props[] = { 0x5d, 0x00, 0x00, 0x10, 0x00 } → lc=3, lp=0, pb=2, dict=1MB
        try:
            lzma_filter = {'id': lzma.FILTER_LZMA1, 'lc': 3, 'lp': 0, 'pb': 2,
                           'dict_size': 0x00100000}
            data = lzma.LZMADecompressor(
                format=lzma.FORMAT_RAW, filters=[lzma_filter]).decompress(
                    compressed_data, max_length=self.hunk_size)
        except lzma.LZMAError:
            pass

        # Fallback: properties encoded in first 5 bytes (lzma-alone format)
        if data is None and len(compressed_data) >= 5:
            try:
                props = compressed_data[0]
                lc = props % 9
                lp = (props // 9) % 5
                pb = props // 45
                dict_size = struct.unpack('<I', compressed_data[1:5])[0]
                lzma_filter = {'id': lzma.FILTER_LZMA1, 'lc': lc, 'lp': lp, 'pb': pb,
                               'dict_size': dict_size if dict_size > 0 else self.hunk_size}
                data = lzma.LZMADecompressor(
                    format=lzma.FORMAT_RAW, filters=[lzma_filter]).decompress(
                        compressed_data[5:], max_length=self.hunk_size)
            except (lzma.LZMAError, struct.error):
                pass

        # Fallback: XZ container format
        if data is None:
            try:
                data = lzma.decompress(compressed_data)
            except lzma.LZMAError:
                pass

        if data is None:
            return b'\x00' * self.hunk_size

        # Clamp to exactly hunk_size (MAME decompresses to exactly destlen)
        if len(data) < self.hunk_size:
            data = data + b'\x00' * (self.hunk_size - len(data))
        elif len(data) > self.hunk_size:
            data = data[:self.hunk_size]
        return data
    
    def _decompress_flac(self, compressed_data: bytes, block_idx: int) -> bytes:
        """Decompress FLAC format as used by MAME CHD.

        MAME stores arbitrary binary data as 16-bit mono FLAC.
        On x86 (little-endian), MAME encodes with swap_endian=True: each pair of
        bytes [A, B] is encoded as sample (A<<8)|B (big-endian read), and decoded
        back by writing the sample's high byte then low byte. So the decoded output
        is the sample values in big-endian byte order, which matches the original data.
        """
        raw = self._flac_decode_to_bytes(compressed_data, block_idx)
        if raw is None:
            return b'\x00' * self.hunk_size
        if len(raw) < self.hunk_size:
            raw += b'\x00' * (self.hunk_size - len(raw))
        return raw[:self.hunk_size]

    def _make_flac_header(self, total_samples: int) -> bytes:
        """Build a synthetic FLAC STREAMINFO header for headerless CHD FLAC frames.

        MAME's FLAC write callback only stores audio frames (skips metadata blocks).
        To decode with standard tools we need to prepend a valid STREAMINFO.

        Parameters from chdcodec.cpp: sample_rate=44100, channels=2, bps=16.
        total_samples = hunk_size // 4  (bytes / 2ch / 2B per sample, per channel).
        Block size varies; use flexible min/max so the decoder uses frame headers.
        """
        sample_rate = 44100
        ch_m1 = 1       # stereo (2 channels - 1)
        bps_m1 = 15     # 16-bit (16 - 1)

        # 8-byte field: [sr:20][ch-1:3][bps-1:5][total_samples:36]
        val = (sample_rate << 44) | (ch_m1 << 41) | (bps_m1 << 36) | (total_samples & 0xFFFFFFFFF)
        sr_ch_bps = val.to_bytes(8, 'big')

        streaminfo = (
            struct.pack('>HH', 16, 65535) +  # min/max block size (accept any valid size)
            b'\x00\x00\x00' +               # min frame size (unknown)
            b'\x00\x00\x00' +               # max frame size (unknown)
            sr_ch_bps +                     # rate/ch/bps/total_samples
            b'\x00' * 16                    # MD5 (unknown)
        )  # 34 bytes

        # Metadata block header: last=1 (0x80), type=STREAMINFO(0), length=34 (0x22)
        return b'fLaC' + b'\x80\x00\x00\x22' + streaminfo

    def _flac_decode_to_bytes(self, compressed_data: bytes, block_idx: int):
        """Decode FLAC data, returning raw PCM bytes or None on failure.

        From chdcodec.cpp chd_flac_decompressor::decompress():
          src[0] == 'L' (0x4C): LE encoding (swap_endian=false on x86) → no byte-swap
          src[0] == 'B' (0x42): BE encoding (swap_endian=true on x86)  → byte-swap int16s
        """
        import array as _array

        if len(compressed_data) < 2:
            return None

        # Strip the mandatory 'L'/'B' endianness marker prepended by MAME
        marker = compressed_data[0]
        if marker == 0x4C:    # 'L': LE encoding, samples are native LE — no swap
            swap = False
            compressed_data = compressed_data[1:]
        elif marker == 0x42:  # 'B': BE encoding, samples are byte-swapped — swap back
            swap = True
            compressed_data = compressed_data[1:]
        else:
            swap = True  # unknown marker: assume BE as fallback

        # total_samples per channel: hunk_size bytes / 2 channels / 2 bytes per sample
        total_samples = self.hunk_size // 4

        # CHD FLAC stores raw frames only (no fLaC/STREAMINFO header); prepend one
        if not compressed_data.startswith(b'fLaC'):
            compressed_data = self._make_flac_header(total_samples) + compressed_data

        try:
            import io
            import soundfile as sf
            data, _ = sf.read(io.BytesIO(compressed_data), dtype='int16', always_2d=True,
                              frames=total_samples)
            raw = data.tobytes()  # C-order: interleaved L/R int16, native (LE) endian
            if swap:
                # Byte-swap each int16: soundfile returns native LE; 'B' hunks need swap
                a = _array.array('h')
                a.frombytes(raw)
                a.byteswap()
                raw = a.tobytes()
            return raw
        except ImportError:
            print("CHD: FLAC support requires 'soundfile' (pip install soundfile)", file=sys.stderr)
        except Exception as e:
            print(f"CHD: FLAC decode error for hunk {block_idx}: {e}", file=sys.stderr)

        return None

    def _decompress_huff(self, compressed_data: bytes, block_idx: int) -> bytes:
        """Decompress MAME huffman_8bit_decoder (huffman_decoder<256,16>).

        Encoder uses export_tree_huffman() (secondary huffman_decoder<24,6>),
        so we must use import_tree_huffman(), NOT import_tree_rle().
        """
        try:
            bitstream = BitstreamReader(compressed_data)
            decoder = HuffmanDecoder(maxbits=16, numcodes=256)
            decoder.import_tree_huffman(bitstream)
            result = bytearray(self.hunk_size)
            for i in range(self.hunk_size):
                result[i] = decoder.decode_one(bitstream)
            return bytes(result)
        except Exception as e:
            print(f"CHD: huff decode error for hunk {block_idx}: {e}", file=sys.stderr)
            return b'\x00' * self.hunk_size

    def _decompress_zlib(self, compressed_data: bytes, block_idx: int) -> bytes:
        """Decompress zlib format."""
        try:
            return zlib.decompress(compressed_data)
        except zlib.error:
            try:
                decompressor = zlib.decompressobj(-zlib.MAX_WBITS)
                return decompressor.decompress(compressed_data)
            except zlib.error:
                return compressed_data.ljust(self.hunk_size, b'\x00')

    @staticmethod
    def detect_chd_codec(file_path: str) -> Optional[Tuple[int, str]]:
        """Detect the compression codec used in a CHD file."""
        try:
            with open(file_path, 'rb') as f:
                header = f.read(64)
                if len(header) < 64:
                    return None
                
                magic = header[0:8]
                if magic != CHD_MAGIC:
                    return None
                
                version = struct.unpack('>I', header[12:16])[0]
                if version != CHD_VERSION:
                    return None
                
                compressors = struct.unpack('>4I', header[16:32])
                
                codec_names = {
                    CHD_CODEC_NONE: 'none',
                    CHD_CODEC_CDLZ: 'cdlz',
                    CHD_CODEC_CDZL: 'cdzl',
                    CHD_CODEC_CDFL: 'cdfl',
                    CHD_CODEC_ZLIB: 'zlib',
                    CHD_CODEC_ZLIB_PLUS: 'zlib+',
                    CHD_CODEC_LZMA: 'lzma',
                }
                
                for codec in compressors:
                    if codec != CHD_CODEC_NONE:
                        return (codec, codec_names.get(codec, f'unknown(0x{codec:08x})'))
                
                return (CHD_CODEC_NONE, 'none')
            
        except (IOError, struct.error):
            return None
