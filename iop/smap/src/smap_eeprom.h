#ifndef SMAP_EEPROM_H
#define SMAP_EEPROM_H


#include "types.h"
#include "smap.h"


void ReadFromEEPROM(SMap* pSMap, u8 u8Addr,u16* pu16Data, int iN);


#endif
