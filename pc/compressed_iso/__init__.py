"""
Compressed ISO file format support for PS2 UDPFS server.

Provides transparent decompression of ZSO (LZ4), CSO (zlib) and CHD compressed ISO files.
"""

from .base import CompressedFileWrapper
from .zso import ZsoFileWrapper
from .cso import CsoFileWrapper
from .chd import ChdFileWrapper, LIBCHDR_AVAILABLE

__all__ = ['CompressedFileWrapper', 'ZsoFileWrapper', 'CsoFileWrapper', 'ChdFileWrapper', 'LIBCHDR_AVAILABLE']
