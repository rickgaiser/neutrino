// File Handle Interface for Files
#ifndef FHI_FILE_H
#define FHI_FILE_H


#include <stdint.h>
#include "fhi.h"


#define FHI_FILE_MAX_LEN 64


struct fhi_file_info
{
    char name[FHI_FILE_MAX_LEN]; /// File name to open, like "mass:file.iso"
    uint64_t size;               /// Size of the file in bytes
} __attribute__((packed));

struct fhi_file
{
    // Magic number to find
    uint32_t magic;

    // Files:
    // 0 = ISO
    struct fhi_file_info file[FHI_MAX_FILES];
} __attribute__((packed));


#endif
