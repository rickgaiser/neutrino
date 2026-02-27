"""
Base class for compressed file format wrappers.

Provides transparent file-like access to block-compressed files with LRU caching.
"""

from collections import OrderedDict
from typing import List


class CompressedFileWrapper:
    """Base class for transparent compressed file access with block caching.
    
    This class provides file-like read/seek/tell/close operations on compressed files,
    handling block decompression transparently with an LRU cache for efficiency.
    
    Subclasses must implement:
        - _parse_header(): Parse format-specific header and populate block_offsets
        - _decompress_block(block_idx): Decompress a single block
    
    Attributes:
        file_path: Path to the compressed file
        uncompressed_size: Size of the uncompressed data in bytes
        block_size: Size of each block in bytes (typically 2048 for PS2 ISOs)
        block_offsets: List of block offsets in the compressed file
    """
    
    DEFAULT_BLOCK_SIZE = 2048
    DEFAULT_CACHE_SIZE = 32  # Number of blocks to cache
    
    def __init__(self, file_path: str, cache_size: int = None):
        """Initialize the compressed file wrapper.
        
        Args:
            file_path: Path to the compressed file
            cache_size: Number of blocks to cache (default: 32)
        """
        self.file_path = file_path
        self.file = open(file_path, 'rb')
        self.pos = 0
        self.uncompressed_size = 0
        self.block_size = self.DEFAULT_BLOCK_SIZE
        self.block_offsets: List[int] = []
        self._cache: OrderedDict = OrderedDict()
        self._cache_size = cache_size or self.DEFAULT_CACHE_SIZE
        self._num_blocks = 0
        
    def read(self, size: int) -> bytes:
        """Read from current position, decompressing blocks as needed.
        
        Args:
            size: Number of bytes to read
            
        Returns:
            Decompressed data (may be less than size if EOF reached)
        """
        if size <= 0:
            return b''
        
        # Clamp to remaining data
        remaining = self.uncompressed_size - self.pos
        if remaining <= 0:
            return b''
        size = min(size, remaining)
        
        result = bytearray()
        while len(result) < size:
            # Calculate current block
            block_idx = self.pos // self.block_size
            block_offset = self.pos % self.block_size
            
            # How much to read from this block
            remaining_to_read = size - len(result)
            available_in_block = self.block_size - block_offset
            read_size = min(remaining_to_read, available_in_block)
            
            # Check if this is the last block (might be partial)
            if block_idx == self._num_blocks - 1:
                last_block_size = self.uncompressed_size % self.block_size
                if last_block_size == 0:
                    last_block_size = self.block_size
                available_in_block = last_block_size - block_offset
                read_size = min(remaining_to_read, available_in_block)
            
            # Decompress block
            block_data = self._get_block(block_idx)
            
            # Extract needed portion
            result.extend(block_data[block_offset:block_offset + read_size])
            self.pos += read_size
        
        return bytes(result)
    
    def seek(self, offset: int, whence: int = 0) -> int:
        """Seek to position in uncompressed stream.
        
        Args:
            offset: Byte offset
            whence: 0=SEEK_SET, 1=SEEK_CUR, 2=SEEK_END
            
        Returns:
            New position in uncompressed stream
        """
        if whence == 0:  # SEEK_SET
            self.pos = offset
        elif whence == 1:  # SEEK_CUR
            self.pos += offset
        elif whence == 2:  # SEEK_END
            self.pos = self.uncompressed_size + offset
        
        # Clamp to valid range
        self.pos = max(0, min(self.pos, self.uncompressed_size))
        return self.pos
    
    def tell(self) -> int:
        """Get current position in uncompressed stream.
        
        Returns:
            Current byte position
        """
        return self.pos
    
    def close(self):
        """Close the underlying file handle."""
        if self.file:
            self.file.close()
            self.file = None
    
    def _get_block(self, block_idx: int) -> bytes:
        """Get a decompressed block, using cache if available.
        
        Args:
            block_idx: Block index (0-based)
            
        Returns:
            Decompressed block data (block_size bytes)
        """
        if block_idx in self._cache:
            # Move to end (most recently used)
            self._cache.move_to_end(block_idx)
            return self._cache[block_idx]
        
        # Decompress and cache
        data = self._decompress_block(block_idx)
        
        # Evict oldest if cache is full
        while len(self._cache) >= self._cache_size:
            self._cache.popitem(last=False)
        
        self._cache[block_idx] = data
        return data
    
    def _decompress_block(self, block_idx: int) -> bytes:
        """Decompress a single block.
        
        Args:
            block_idx: Block index (0-based)
            
        Returns:
            Decompressed block data
            
        Raises:
            NotImplementedError: Must be overridden in subclass
        """
        raise NotImplementedError
    
    def _parse_header(self):
        """Parse format-specific header and populate block_offsets.
        
        Must set:
            - self.uncompressed_size
            - self.block_size
            - self.block_offsets
            - self._num_blocks
            
        Raises:
            NotImplementedError: Must be overridden in subclass
        """
        raise NotImplementedError
