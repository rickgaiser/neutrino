"""
CHD (Compressed Hunks of Data) format wrapper.

Supports CHD v5 format used by MAME and PCSX2.
Implements zlib and lzma decompression (most common for PS2 ISOs).

CHD uses "hunks" which are similar to blocks in CSO/ZSO.
Each hunk can be compressed with different codecs.

Based on MAME chd.cpp/chdcodec.cpp source code.
"""

import struct
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
    """Read bits from a byte stream."""
    
    def __init__(self, data: bytes):
        self.data = data
        self.offset = 0  # bit offset
        
    def read(self, num_bits: int) -> int:
        """Read num_bits from the stream."""
        if num_bits == 0:
            return 0
            
        result = 0
        bits_read = 0
        
        while bits_read < num_bits:
            byte_offset = self.offset // 8
            bit_offset = self.offset % 8
            
            if byte_offset >= len(self.data):
                raise ValueError("Bitstream overflow")
            
            # How many bits can we read from current byte?
            bits_available = 8 - bit_offset
            bits_needed = num_bits - bits_read
            bits_to_read = min(bits_available, bits_needed)
            
            # Extract bits from current byte
            byte_val = self.data[byte_offset]
            mask = (1 << bits_to_read) - 1
            shift = bits_available - bits_to_read
            bits = (byte_val >> shift) & mask
            
            result = (result << bits_to_read) | bits
            bits_read += bits_to_read
            self.offset += bits_to_read
        
        return result
    
    def remaining(self) -> int:
        """Return number of bits remaining."""
        return len(self.data) * 8 - self.offset


class HuffmanDecoder:
    """Huffman decoder for CHD v5 map.
    
    Based on MAME huffman.cpp import_tree_rle().
    """
    
    def __init__(self, maxbits: int = 16, numcodes: int = 256):
        self.maxbits = maxbits
        self.numcodes = numcodes
        # numbits for reading tree: 5 for maxbits>=16, 4 for maxbits>=8, 3 otherwise
        self.numbits = 5 if maxbits >= 16 else (4 if maxbits >= 8 else 3)
        # huffnode[i] = {'numbits': bit_length, 'code': canonical_code}
        self.huffnode = [{'numbits': 0, 'code': 0} for _ in range(numcodes)]
        self.lookup = {}  # (length, code) -> value
        
    def import_tree_rle(self, bitstream: BitstreamReader) -> None:
        """Import Huffman tree from RLE-encoded format.

        From MAME huffman.cpp import_tree_rle():
        - Read numbits per entry
        - If value != 1: raw numbits value
        - If value == 1:
          - If next value == 1: single 1
          - Else: repeat (next value + 3) times
        """
        curnode = 0
        while curnode < self.numcodes:
            nodebits = bitstream.read(self.numbits)
            if nodebits != 1:
                # Raw value
                self.huffnode[curnode]['numbits'] = nodebits
                curnode += 1
            else:
                # Escape code - read next value
                nodebits2 = bitstream.read(self.numbits)
                if nodebits2 == 1:
                    # Double 1 = single 1
                    self.huffnode[curnode]['numbits'] = 1
                    curnode += 1
                else:
                    # Repeat count: read another value and add 3
                    repcount = bitstream.read(self.numbits) + 3
                    while repcount > 0 and curnode < self.numcodes:
                        self.huffnode[curnode]['numbits'] = nodebits2
                        curnode += 1
                        repcount -= 1
        
        # Verify we got the right number
        if curnode != self.numcodes:
            raise ValueError(f"Huffman tree incomplete: got {curnode}, expected {self.numcodes}")
        
        # Assign canonical codes
        self._assign_canonical_codes()
        
        # Build lookup table
        self._build_lookup_table()
    
    def _assign_canonical_codes(self) -> None:
        """Assign canonical Huffman codes based on bit lengths."""
        # Count nodes at each bit length
        bithistogram = [0] * (self.maxbits + 2)  # +2 for safety
        for node in self.huffnode:
            bits = node['numbits']
            if bits > 0 and bits <= self.maxbits:
                bithistogram[bits] += 1
        
        # Compute starting code for each bit length
        # Canonical Huffman: codes of same length are consecutive
        startcode = [0] * (self.maxbits + 2)
        for i in range(self.maxbits, 0, -1):
            startcode[i - 1] = (startcode[i] + bithistogram[i]) >> 1
        
        # Assign codes to nodes
        for node in self.huffnode:
            bits = node['numbits']
            if bits > 0 and bits <= self.maxbits:
                node['code'] = startcode[bits]
                startcode[bits] += 1
    
    def _build_lookup_table(self) -> None:
        """Build lookup table for fast decoding."""
        self.lookup = {}
        for i, node in enumerate(self.huffnode):
            bits = node['numbits']
            if bits > 0:
                self.lookup[(bits, node['code'])] = i
    
    def decode_one(self, bitstream: BitstreamReader) -> int:
        """Decode one value from the bitstream."""
        code = 0
        for bits in range(1, self.maxbits + 1):
            if bitstream.remaining() < 1:
                return 0
            code = (code << 1) | bitstream.read(1)
            if (bits, code) in self.lookup:
                return self.lookup[(bits, code)]
        return 0


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
        else:
            # Unknown codec, return as-is
            return compressed_data[:self.block_size].ljust(self.block_size, b'\x00')
    
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
        """Decompress standard LZMA format."""
        try:
            return lzma.decompress(compressed_data)
        except lzma.LZMAError:
            return compressed_data.ljust(self.hunk_size, b'\x00')
    
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
