// File Handle Interface for file ID's (already opened files)
#ifndef FHI_FILEID_H
#define FHI_FILEID_H


#include <stdint.h>
#include "fhi.h"


struct fhi_fileid_info
{
    int id;        /// File ID of already opened file
    uint64_t size; /// Size of the file in bytes
} __attribute__((packed));

struct fhi_fileid
{
    // Magic number to find
    uint32_t magic;

    uint32_t devNr;   /// Device number: 0, 1, 2, etc...

    // Files:
    // 0 = ISO
    struct fhi_fileid_info file[FHI_MAX_FILES];
} __attribute__((packed));

#endif