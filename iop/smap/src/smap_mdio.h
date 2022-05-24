#ifndef SMAP_MDIO_H
#define SMAP_MDIO_H


#include "types.h"
#include "smap.h"


int smap_mdio_read (SMap* pSMap, u32 u32PhyAddr, u32 u32RegAddr);
int smap_mdio_write(SMap* pSMap, u32 u32PhyAddr, u32 u32RegAddr, u16 u16Data);


#endif
