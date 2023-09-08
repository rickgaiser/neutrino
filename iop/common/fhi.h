// File Handle Interface
// For other modules to access neutrino/OPL files
// Like ISO / VMC / ...
#ifndef FHI_H
#define FHI_H


#include <stdint.h>
#include <irx.h>

// Size of the file in SECTORS of 512b
uint32_t fhi_size(int file_handle);
// Read SECTORS from file
int fhi_read(int file_handle, void *buffer, unsigned int sector_start, unsigned int sector_count);
// Write SECTORS to file
int fhi_write(int file_handle, const void *buffer, unsigned int sector_start, unsigned int sector_count);

#define fhi_IMPORTS_start DECLARE_IMPORT_TABLE(fhi, 1, 1)
#define fhi_IMPORTS_end END_IMPORT_TABLE

#define I_fhi_size DECLARE_IMPORT(4, fhi_size)
#define I_fhi_read DECLARE_IMPORT(5, fhi_read)
#define I_fhi_write DECLARE_IMPORT(6, fhi_write)


#endif