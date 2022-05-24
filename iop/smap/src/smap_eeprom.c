#include <thbase.h>
#include <intrman.h>

#include "smap.h"
#include "smap_eeprom.h"


#define	SMAP_EEPROM_WRITE_WAIT		100000
#define	SMAP_PP_GET_Q(pSMap)		((SMAP_REG8((pSMap),SMAP_PIOPORT_IN)>>4)&1)
#define	SMAP_PP_SET_D(pSMap,D)		((pSMap)->u8PPWC=(D) ? ((pSMap)->u8PPWC|PP_DIN):((pSMap)->u8PPWC&~PP_DIN))
#define	SMAP_PP_SET_S(pSMap,S)		((pSMap)->u8PPWC=(S) ? ((pSMap)->u8PPWC|PP_CSEL):((pSMap)->u8PPWC&~PP_CSEL))
#define	SMAP_PP_CLK_OUT(pSMap,C)	{(pSMap)->u8PPWC=(C) ? ((pSMap)->u8PPWC|PP_SCLK):((pSMap)->u8PPWC&~PP_SCLK); \
					 SMAP_REG8((pSMap),SMAP_PIOPORT_OUT)=(pSMap)->u8PPWC;}


/*
 * private functions
 */
static inline void
EEPROMClockDataOut(SMap* pSMap, int val)
{
	SMAP_PP_SET_D(pSMap, val);

	SMAP_PP_CLK_OUT(pSMap, 0);
	DelayThread(1);	//tDIS

	SMAP_PP_CLK_OUT(pSMap, 1);
	DelayThread(1);	//tSKH, tDIH

	SMAP_PP_CLK_OUT(pSMap, 0);
	DelayThread(1);	//tSKL
}

static inline int
EEPROMClockDataIn(SMap* pSMap)
{
	int iRet;

	SMAP_PP_SET_D(pSMap, 0);
	SMAP_PP_CLK_OUT(pSMap, 0);
	DelayThread(1);	//tSKL

	SMAP_PP_CLK_OUT(pSMap, 1);
	DelayThread(1);	//tSKH, tPD0,1
	iRet=SMAP_PP_GET_Q(pSMap);

	SMAP_PP_CLK_OUT(pSMap, 0);
	DelayThread(1);	//tSKL

	return iRet;
}

static void
EEPROMPutAddr(SMap* pSMap, u8 u8Addr)
{
	int iA;

	u8Addr &= 0x3f;
	for(iA=0; iA<6; ++iA) {
		EEPROMClockDataOut(pSMap, (u8Addr & 0x20) ? 1 : 0);
		u8Addr <<= 1;
	}
}

static u16
GetDataFromEEPROM(SMap* pSMap)
{
	int iA;
	u16 u16Data = 0;

	for(iA=0; iA<16; ++iA) {
		u16Data <<= 1;
		u16Data |= EEPROMClockDataIn(pSMap);
	}
	return u16Data;
}

static void
EEPROMStartOp(SMap* pSMap, int iOp)
{

	//Set port direction.

	SMAP_REG8(pSMap, SMAP_PIOPORT_DIR) = (PP_SCLK|PP_CSEL|PP_DIN);

	//Rise chip select.

	SMAP_PP_SET_S(pSMap, 0);
	SMAP_PP_SET_D(pSMap, 0);
	SMAP_PP_CLK_OUT(pSMap, 0);
	DelayThread(1);	//tSKS

	SMAP_PP_SET_S(pSMap, 1);
	SMAP_PP_SET_D(pSMap, 0);
	SMAP_PP_CLK_OUT(pSMap, 0);
	DelayThread(1);	//tCSS

	//Put start bit.

	EEPROMClockDataOut(pSMap, 1);

	//Put op code.

	EEPROMClockDataOut(pSMap,(iOp>>1) & 1);
	EEPROMClockDataOut(pSMap,iOp & 1);
}

static void
EEPROMCSLow(SMap* pSMap)
{
	SMAP_PP_SET_S(pSMap, 0);
	SMAP_PP_SET_D(pSMap, 0);
	SMAP_PP_CLK_OUT(pSMap, 0);
	DelayThread(2);	//tSLSH
}

static void
ReadEEPROMExec(SMap* pSMap, u8 u8Addr, u16* pu16Data, int iN)
{
	int iA;

	EEPROMStartOp(pSMap, PP_OP_READ);
	EEPROMPutAddr(pSMap, u8Addr);
	for(iA=0; iA<iN; ++iA) {
		*pu16Data++ = GetDataFromEEPROM(pSMap);
	}
	EEPROMCSLow(pSMap);
}

/*
 * public functions
 */
void
ReadFromEEPROM(SMap* pSMap, u8 u8Addr, u16* pu16Data, int iN)
{
	int flags;

	CpuSuspendIntr(&flags);
	ReadEEPROMExec(pSMap, u8Addr, pu16Data, iN);
	CpuResumeIntr(flags);
}
