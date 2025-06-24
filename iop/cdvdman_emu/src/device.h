#ifndef DEVICE_H
#define DEVICE_H

#include <stdint.h>

uint32_t fhi_get_lsn();
int fhi_read_sectors(uint32_t lsn, void *buffer, unsigned int sectors);

#endif
