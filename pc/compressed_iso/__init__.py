"""
Compressed ISO file format support for PS2 UDPFS server.

Provides transparent decompression of ZSO (LZ4) and CSO (zlib) compressed ISO files.
"""

from .base import CompressedFileWrapper
from .zso import ZsoFileWrapper
from .cso import CsoFileWrapper

__all__ = ['CompressedFileWrapper', 'ZsoFileWrapper', 'CsoFileWrapper']
