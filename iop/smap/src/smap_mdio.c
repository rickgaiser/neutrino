#include "smap.h"
#include "smap_mdio.h"


/*
 * private functions
 */
static inline int
smap_mdio_busy_wait(SMap* pSMap)
{
	int iA;

	for (iA=SMAP_LOOP_COUNT; iA!=0; --iA) {
		if (EMAC3REG_READ(pSMap, SMAP_EMAC3_STA_CTRL) & E3_PHY_OP_COMP) {
			return 0;
		}
	}

	return -1;
}

/*
 * public functions
 */
int
smap_mdio_read(SMap* pSMap, u32 u32PhyAddr, u32 u32RegAddr)
{
	//Check complete bit
	if (smap_mdio_busy_wait(pSMap))
		return -1;

	//Write phy address and register address
	EMAC3REG_WRITE(pSMap, SMAP_EMAC3_STA_CTRL, E3_PHY_READ |
						   ((u32PhyAddr & E3_PHY_ADDR_MSK) << E3_PHY_ADDR_BITSFT) |
						    (u32RegAddr & E3_PHY_REG_ADDR_MSK));

	//Check complete bit
	if (smap_mdio_busy_wait(pSMap))
		return -1;

	//Workaround: it may be needed to re-read to get correct phy data
	return EMAC3REG_READ(pSMap, SMAP_EMAC3_STA_CTRL) >> E3_PHY_DATA_BITSFT;
}

int
smap_mdio_write(SMap* pSMap, u32 u32PhyAddr, u32 u32RegAddr, u16 u16Data)
{
	//Check complete bit
	if (smap_mdio_busy_wait(pSMap))
		return -1;

	//Write data, phy address and register address.
	EMAC3REG_WRITE(pSMap, SMAP_EMAC3_STA_CTRL, E3_PHY_WRITE |
						   ((u16Data    & E3_PHY_DATA_MSK) << E3_PHY_DATA_BITSFT) |
						   ((u32PhyAddr & E3_PHY_ADDR_MSK) << E3_PHY_ADDR_BITSFT) |
			                            (u32RegAddr & E3_PHY_REG_ADDR_MSK));

	//Check complete bit
	if (smap_mdio_busy_wait(pSMap))
		return -1;

	return 0;
}
