"""
CHD (Compressed Hunks of Data) format wrapper.

Supports CHD v5 format used by MAME and PCSX2.
Uses libchdr (the official MAME C library) for decompression.

Install: apt install libchdr0
Build from source: https://github.com/rtissera/libchdr
"""

import ctypes
import ctypes.util
import struct

from .base import CompressedFileWrapper


# ---------------------------------------------------------------------------
# libchdr loading
# ---------------------------------------------------------------------------

def _load_libchdr():
    """Attempt to load libchdr. Returns loaded ctypes.CDLL or raises ImportError."""
    name = ctypes.util.find_library("chdr")
    candidates = ["libchdr.so.0", "libchdr.so"]
    if name:
        candidates.insert(0, name)
    for candidate in candidates:
        try:
            lib = ctypes.cdll.LoadLibrary(candidate)
            _declare_api(lib)
            return lib
        except OSError:
            continue
    raise ImportError(
        "libchdr not found. Install with:\n"
        "  apt install libchdr0\n"
        "or build from source: https://github.com/rtissera/libchdr"
    )


def _declare_api(lib):
    """Declare ctypes signatures for the four libchdr functions we use."""
    class _chd_file(ctypes.Structure):
        pass
    _p  = ctypes.POINTER(_chd_file)
    _pp = ctypes.POINTER(ctypes.POINTER(_chd_file))
    lib._chd_file_p  = _p
    lib._chd_file_pp = _pp
    lib.chd_open.argtypes  = [ctypes.c_char_p, ctypes.c_int, _p, _pp]
    lib.chd_open.restype   = ctypes.c_int
    lib.chd_read.argtypes  = [_p, ctypes.c_uint32, ctypes.c_void_p]
    lib.chd_read.restype   = ctypes.c_int
    lib.chd_close.argtypes = [_p]
    lib.chd_close.restype  = None


try:
    _lib = _load_libchdr()
    LIBCHDR_AVAILABLE = True
    _libchdr_error = None
except ImportError as _e:
    _lib = None
    LIBCHDR_AVAILABLE = False
    _libchdr_error = str(_e)

CHD_OPEN_READ = 1


# ---------------------------------------------------------------------------
# CD sector extraction
# ---------------------------------------------------------------------------

# CD codec IDs: cdlz, cdzl, cdfl
_CD_CODECS     = frozenset([0x63646c7a, 0x63647a6c, 0x6364666c])
_CD_FRAME_SIZE = 2448   # 2352-byte sector + 96-byte subcode
_USER_DATA     = 2048
_CD_SYNC       = bytes([0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
                         0xff, 0xff, 0xff, 0xff, 0xff, 0x00])


def _extract_user_data(sector: bytes) -> bytes:
    """Extract 2048 bytes of ISO user data from a 2352-byte raw CD sector."""
    if len(sector) < _USER_DATA:
        return bytes(sector).ljust(_USER_DATA, b'\x00')
    if len(sector) >= 16 and sector[0:12] == _CD_SYNC:
        # Mode 2 Form 1 (PS1/PS2 CD): user data at bytes 24-2071
        # Mode 1: user data at bytes 16-2063
        offset = 24 if sector[15] == 2 else 16
    else:
        # No sync header — libchdr stripped it; data starts at byte 0
        offset = 0
    end = offset + _USER_DATA
    if end > len(sector):
        return bytes(sector[offset:]).ljust(_USER_DATA, b'\x00')
    return bytes(sector[offset:end])


# ---------------------------------------------------------------------------
# ChdFileWrapper
# ---------------------------------------------------------------------------

class ChdFileWrapper(CompressedFileWrapper):
    """CHD v5 wrapper backed by libchdr (apt install libchdr0).

    Parses the CHD v5 header directly in Python (stable binary format),
    then delegates all hunk decompression to chd_read() from libchdr.
    Supports both HD-type (lzma/zlib/huff/flac) and CD-type (cdlz/cdzl/cdfl) CHDs.
    """

    def __init__(self, file_path: str, cache_size: int = None):
        if not LIBCHDR_AVAILABLE:
            raise ImportError(_libchdr_error)
        super().__init__(file_path, cache_size)
        self._chd_handle      = None
        self._is_cd_format    = False
        self._frames_per_hunk = 0
        self._hunk_buf        = None
        try:
            self._parse_header()
        except Exception:
            self.close()
            raise

    def _parse_header(self):
        # Parse CHD v5 header bytes directly — format is stable across libchdr versions
        self.file.seek(0)
        header = self.file.read(124)
        if len(header) < 64 or header[0:8] != b'MComprHD':
            raise ValueError("Not a valid CHD file")
        version = struct.unpack('>I', header[12:16])[0]
        if version != 5:
            raise ValueError(f"Unsupported CHD version {version} (only v5 supported)")

        compressors   = list(struct.unpack('>4I', header[16:32]))
        logical_bytes = struct.unpack('>Q', header[32:40])[0]
        hunk_size     = struct.unpack('>I', header[56:60])[0]
        unit_size     = struct.unpack('>I', header[60:64])[0]

        if hunk_size == 0:
            raise ValueError("CHD hunkbytes is 0")

        self.hunk_size   = hunk_size
        self.block_size  = hunk_size
        self._num_blocks = (logical_bytes + hunk_size - 1) // hunk_size

        # CD-format detection: cdlz/cdzl/cdfl codecs + 2448-byte units
        self._is_cd_format = (
            unit_size == _CD_FRAME_SIZE
            and any(c in _CD_CODECS for c in compressors if c != 0)
        )
        if self._is_cd_format and hunk_size % _CD_FRAME_SIZE == 0:
            self._frames_per_hunk  = hunk_size // _CD_FRAME_SIZE
            total_frames           = self._num_blocks * self._frames_per_hunk
            self.uncompressed_size = total_frames * _USER_DATA
            self.block_size        = self._frames_per_hunk * _USER_DATA
        else:
            self.uncompressed_size = logical_bytes

        # Open CHD via libchdr (it opens its own file descriptor internally)
        handle = _lib._chd_file_p()
        err = _lib.chd_open(self.file_path.encode(), CHD_OPEN_READ,
                            None, ctypes.byref(handle))
        if err != 0:
            raise IOError(f"chd_open failed (error {err}) for '{self.file_path}'")
        self._chd_handle = handle
        self._hunk_buf   = ctypes.create_string_buffer(hunk_size)

    def _decompress_block(self, block_idx: int) -> bytes:
        err = _lib.chd_read(self._chd_handle, block_idx, self._hunk_buf)
        if err != 0:
            raise IOError(f"CHD: chd_read failed for hunk {block_idx} (error {err})")
        raw = self._hunk_buf.raw
        if not self._is_cd_format:
            return bytes(raw)
        # CD format: extract 2048-byte user data from each 2352-byte sector in the hunk
        result = bytearray()
        for i in range(self._frames_per_hunk):
            sector = raw[i * _CD_FRAME_SIZE : i * _CD_FRAME_SIZE + 2352]
            result.extend(_extract_user_data(sector))
        return bytes(result)

    def close(self):
        if self._chd_handle is not None:
            _lib.chd_close(self._chd_handle)
            self._chd_handle = None
        super().close()
