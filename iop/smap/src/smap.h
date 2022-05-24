/* ps2-load-ip

   smap.h

   Copyright (c)2001 Sony Computer Entertainment Inc.
   Copyright (c)2001 YAEGASHI Takeshi
   Copyright (2)2002 Dan Potter
   Copyright (c)2003 T Lindstrom
   License: GPL

   $Id: smap.h,v 1.1 2009/07/19 22:30:51 kloader Exp $
*/

/*
 *  smap.h -- PlayStation 2 Ethernet device driver header file
 *
 *	Copyright (C) 2001  Sony Computer Entertainment Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General
 *  Public License Version 2. See the file "COPYING" in the main
 *  directory of this archive for more details.
 *
 * Modified for ps2-load-ip by Dan Potter
 */

#ifndef	__SMAP_H__
#define	__SMAP_H__

#include "types.h"


//Flags
#define	SMAP_F_OPENED			(1<<0)
//#define	SMAP_F_LINKESTABLISH		(1<<1)
#define	SMAP_F_LINKVALID		(1<<2)
//#define	SMAP_F_CHECK_FORCE100M		(1<<3)
//#define	SMAP_F_CHECK_FORCE10M		(1<<4)
//#define	SMAP_F_TXDNV_DISABLE		(1<<16)
//#define	SMAP_F_RXDNV_DISABLE		(1<<17)
//#define	SMAP_F_PRINT_PKT		(1<<30)
//#define	SMAP_F_PRINT_MSG		(1<<31)

#define	SMAP_TXBUFBASE			0x1000
#define	SMAP_TXBUFSIZE			(4*1024)
#define	SMAP_RXBUFBASE			0x4000
#define	SMAP_RXBUFSIZE			(16*1024)

#define	SMAP_ALIGN			16
#define	SMAP_TXMAXSIZE			(6+6+2+1500)
#define	SMAP_TXMAXTAILPAD		4
#define	SMAP_TXMAXPKTSZ_INFIFO		(SMAP_TXMAXSIZE+2)	//Multiple of 4
#define	SMAP_RXMAXSIZE			(6+6+2+1500+4)
#define	SMAP_RXMINSIZE			14							//Ethernet header size
#define	SMAP_RXMAXTAILPAD		4

#define	SMAP_LOOP_COUNT			10000

//Buffer Descriptor(BD) Offset and Definitions
#define	SMAP_BD_BASE			0x3000
#define	SMAP_BD_BASE_TX			(SMAP_BD_BASE+0x0000)
#define	SMAP_BD_BASE_RX			(SMAP_BD_BASE+0x0200)
#define	SMAP_BD_SIZE			512
#define	SMAP_BD_MAX_ENTRY		64

#define	SMAP_BD_NEXT(x)	(x)=(x)<(SMAP_BD_MAX_ENTRY-1) ? (x)+1:0

//TX Control
#define	SMAP_BD_TX_READY		(1<<15)	//set:driver, clear:HW
#define	SMAP_BD_TX_GENFCS		(1<<9)	//generate FCS
#define	SMAP_BD_TX_GENPAD		(1<<8)	//generate padding
#define	SMAP_BD_TX_INSSA		(1<<7)	//insert source address
#define	SMAP_BD_TX_RPLSA		(1<<6)	//replace source address
#define	SMAP_BD_TX_INSVLAN		(1<<5)	//insert VLAN Tag
#define	SMAP_BD_TX_RPLVLAN		(1<<4)	//replace VLAN Tag

//TX Status
#define	SMAP_BD_TX_READY		(1<<15)	//set:driver, clear:HW
#define	SMAP_BD_TX_BADFCS		(1<<9)	//bad FCS
#define	SMAP_BD_TX_BADPKT		(1<<8)	//bad previous pkt in dependent mode
#define	SMAP_BD_TX_LOSSCR		(1<<7)	//loss of carrior sense
#define	SMAP_BD_TX_EDEFER		(1<<6)	//excessive deferal
#define	SMAP_BD_TX_ECOLL		(1<<5)	//excessive collision
#define	SMAP_BD_TX_LCOLL		(1<<4)	//late collision
#define	SMAP_BD_TX_MCOLL		(1<<3)	//multiple collision
#define	SMAP_BD_TX_SCOLL		(1<<2)	//single collision
#define	SMAP_BD_TX_UNDERRUN		(1<<1)	//underrun
#define	SMAP_BD_TX_SQE			(1<<0)	//SQE
#define	SMAP_BD_TX_ERRMASK		(SMAP_BD_TX_BADFCS|SMAP_BD_TX_BADPKT|SMAP_BD_TX_LOSSCR|SMAP_BD_TX_EDEFER|SMAP_BD_TX_ECOLL|	\
					 SMAP_BD_TX_LCOLL|SMAP_BD_TX_MCOLL|SMAP_BD_TX_SCOLL|SMAP_BD_TX_UNDERRUN|SMAP_BD_TX_SQE)

//RX Control
#define	SMAP_BD_RX_EMPTY		(1<<15)	//set:driver, clear:HW

//RX Status
#define	SMAP_BD_RX_EMPTY		(1<<15)	//set:driver, clear:HW
#define	SMAP_BD_RX_OVERRUN		(1<<9)	//overrun
#define	SMAP_BD_RX_PFRM			(1<<8)	//pause frame
#define	SMAP_BD_RX_BADFRM		(1<<7)	//bad frame
#define	SMAP_BD_RX_RUNTFRM		(1<<6)	//runt frame
#define	SMAP_BD_RX_SHORTEVNT		(1<<5)	//short event
#define	SMAP_BD_RX_ALIGNERR		(1<<4)	//alignment error
#define	SMAP_BD_RX_BADFCS		(1<<3)	//bad FCS
#define	SMAP_BD_RX_FRMTOOLONG		(1<<2)	//frame too long
#define	SMAP_BD_RX_OUTRANGE		(1<<1)	//out of range error
#define	SMAP_BD_RX_INRANGE		(1<<0)	//in range error
#define	SMAP_BD_RX_ERRMASK		(SMAP_BD_RX_OVERRUN|SMAP_BD_RX_PFRM|SMAP_BD_RX_BADFRM|SMAP_BD_RX_RUNTFRM|SMAP_BD_RX_SHORTEVNT|	\
											 SMAP_BD_RX_ALIGNERR|SMAP_BD_RX_BADFCS|SMAP_BD_RX_FRMTOOLONG|SMAP_BD_RX_OUTRANGE|					\
											 SMAP_BD_RX_INRANGE)


struct smapbd
{
	u16		ctrl_stat;
	u16		reserved;	//Must be zero
	u16		length;		//Number of bytes in pkt
	u16		pointer;
};

typedef struct smapbd volatile	SMapBD;


typedef struct SMapCircularBuffer
{
	SMapBD*		pBD;
	u16		u16PTRStart;
	u16		u16PTREnd;
	u8		u8IndexStart;
	u8		u8IndexDMA;
	u8		u8IndexEnd;
	u16		u16Size;
} SMapCB;


typedef struct SMap
{
	u32		u32Flags;
	u8 volatile*	pu8Base;
	u8		au8HWAddr[6];
	u32		u32TXMode;
	u8		u8PPWC;
	SMapCB		TX;
	u8		u8RXIndex;
	SMapBD*		pRXBD;
} SMap;


//Register Offset and Definitions
#define	SMAP_PIOPORT_DIR		0x2C
#define	SMAP_PIOPORT_IN			0x2E
#define	SMAP_PIOPORT_OUT		0x2E
#define	PP_DOUT				(1<<4)	//Data output, read port
#define	PP_DIN				(1<<5)	//Data input,  write port
#define	PP_SCLK				(1<<6)	//Clock,       write port
#define	PP_CSEL				(1<<7)	//Chip select, write port
//operation code
#define	PP_OP_READ			2	//2b'10
#define	PP_OP_WRITE			1	//2b'01
#define	PP_OP_EWEN			0	//2b'00
#define	PP_OP_EWDS			0	//2b'00

#define	SMAP_BASE			0xb0000000

#define SPD_R_REV_1                     0x02

#define	SMAP_INTR_STAT			0x28
#define	SMAP_INTR_CLR			0x128
#define	SMAP_INTR_ENABLE		0x2A
#define	SMAP_BD_MODE			0x102
#define	BD_SWAP				(1<<0)

#define	SMAP_TXFIFO_CTRL		0x1000
#define	TXFIFO_RESET			(1<<0)
#define	TXFIFO_DMAEN			(1<<1)
#define	SMAP_TXFIFO_WR_PTR		0x1004
#define	SMAP_TXFIFO_SIZE		0x1008
#define	SMAP_TXFIFO_FRAME_CNT		0x100C
#define	SMAP_TXFIFO_FRAME_INC		0x1010
#define	SMAP_TXFIFO_DATA		0x1100

#define	SMAP_RXFIFO_CTRL		0x1030
#define	RXFIFO_RESET			(1<<0)
#define	RXFIFO_DMAEN			(1<<1)
#define	SMAP_RXFIFO_RD_PTR		0x1034
#define	SMAP_RXFIFO_SIZE		0x1038
#define	SMAP_RXFIFO_FRAME_CNT		0x103C
#define	SMAP_RXFIFO_FRAME_DEC		0x1040
#define	SMAP_RXFIFO_DATA		0x1200

#define	SMAP_FIFO_ADDR			0x1300
#define	FIFO_CMD_READ			(1<<1)
#define	FIFO_DATA_SWAP			(1<<0)
#define	SMAP_FIFO_DATA			0x1308

#define	SMAP_REG8(pSMap,Offset)		(*(u8 volatile*)((pSMap)->pu8Base+(Offset)))
#define	SMAP_REG16(pSMap,Offset)	(*(u16 volatile*)((pSMap)->pu8Base+(Offset)))
#define	SMAP_REG32(pSMap,Offset)	(*(u32 volatile*)((pSMap)->pu8Base+(Offset)))

static inline u32
EMAC3REG_READ(SMap* pSMap,u32 u32Offset)
{
	u32	hi=SMAP_REG16(pSMap,u32Offset);
	u32	lo=SMAP_REG16(pSMap,u32Offset+2);
	return	(hi<<16)|lo;
}

static inline void
EMAC3REG_WRITE(SMap* pSMap,u32 u32Offset,u32 u32V)
{
	SMAP_REG16(pSMap,u32Offset)=((u32V>>16)&0xFFFF);
	SMAP_REG16(pSMap,u32Offset+2)=(u32V&0xFFFF);
}

//EMAC3 Register Offset and Definitions
#define	SMAP_EMAC3_BASE			0x2000
#define	SMAP_EMAC3_MODE0		(SMAP_EMAC3_BASE+0x00)
#define	E3_RXMAC_IDLE			(1<<31)
#define	E3_TXMAC_IDLE			(1<<30)
#define	E3_SOFT_RESET			(1<<29)
#define	E3_TXMAC_ENABLE			(1<<28)
#define	E3_RXMAC_ENABLE			(1<<27)
#define	E3_WAKEUP_ENABLE		(1<<26)

#define	SMAP_EMAC3_MODE1		(SMAP_EMAC3_BASE+0x04)
#define	E3_FDX_ENABLE			(1<<31)
#define	E3_INLPBK_ENABLE		(1<<30)	//internal loop back
#define	E3_VLAN_ENABLE			(1<<29)
#define	E3_FLOWCTRL_ENABLE		(1<<28)	//integrated flow ctrl(pause frame)
#define	E3_ALLOW_PF			(1<<27)	//allow pause frame
#define	E3_ALLOW_EXTMNGIF		(1<<25)	//allow external management IF
#define	E3_IGNORE_SQE			(1<<24)
#define	E3_MEDIA_FREQ_BITSFT		(22)
#define	E3_MEDIA_10M			(0<<22)
#define	E3_MEDIA_100M			(1<<22)
#define	E3_MEDIA_1000M			(2<<22)
#define	E3_MEDIA_MSK			(3<<22)
#define	E3_RXFIFO_SIZE_BITSFT		(20)
#define	E3_RXFIFO_512			(0<<20)
#define	E3_RXFIFO_1K			(1<<20)
#define	E3_RXFIFO_2K			(2<<20)
#define	E3_RXFIFO_4K			(3<<20)
#define	E3_RXFIFO_16K			(0<<20)
#define	E3_TXFIFO_SIZE_BITSFT		(18)
#define	E3_TXFIFO_512			(0<<18)
#define	E3_TXFIFO_1K			(1<<18)
#define	E3_TXFIFO_2K			(2<<18)
#define	E3_TXREQ0_BITSFT		(15)
#define	E3_TXREQ0_SINGLE		(0<<15)
#define	E3_TXREQ0_MULTI			(1<<15)
#define	E3_TXREQ0_DEPEND		(2<<15)
#define	E3_TXREQ1_BITSFT		(13)
#define	E3_TXREQ1_SINGLE		(0<<13)
#define	E3_TXREQ1_MULTI			(1<<13)
#define	E3_TXREQ1_DEPEND		(2<<13)
#define	E3_JUMBO_ENABLE			(1<<12)

#define	SMAP_EMAC3_TxMODE0		(SMAP_EMAC3_BASE+0x08)
#define	E3_TX_GNP_0			(1<<31)	//get new packet
#define	E3_TX_GNP_1			(1<<30)	//get new packet
#define	E3_TX_GNP_DEPEND		(1<<29)	//get new packet
#define	E3_TX_FIRST_CHANNEL		(1<<28)

#define	SMAP_EMAC3_TxMODE1		(SMAP_EMAC3_BASE+0x0C)
#define	E3_TX_LOW_REQ_MSK		(0x1F)	//low priority request
#define	E3_TX_LOW_REQ_BITSFT		(27)		//low priority request
#define	E3_TX_URG_REQ_MSK		(0xFF)	//urgent priority request
#define	E3_TX_URG_REQ_BITSFT		(16)		//urgent priority request

#define	SMAP_EMAC3_RxMODE		(SMAP_EMAC3_BASE+0x10)
#define	E3_RX_STRIP_PAD			(1<<31)
#define	E3_RX_STRIP_FCS			(1<<30)
#define	E3_RX_RX_RUNT_FRAME		(1<<29)
#define	E3_RX_RX_FCS_ERR		(1<<28)
#define	E3_RX_RX_TOO_LONG_ERR		(1<<27)
#define	E3_RX_RX_IN_RANGE_ERR		(1<<26)
#define	E3_RX_PROP_PF			(1<<25)	//propagate pause frame
#define	E3_RX_PROMISC			(1<<24)
#define	E3_RX_PROMISC_MCAST		(1<<23)
#define	E3_RX_INDIVID_ADDR		(1<<22)
#define	E3_RX_INDIVID_HASH		(1<<21)
#define	E3_RX_BCAST			(1<<20)
#define	E3_RX_MCAST			(1<<19)

#define	SMAP_EMAC3_INTR_STAT		(SMAP_EMAC3_BASE+0x14)
#define	SMAP_EMAC3_INTR_ENABLE		(SMAP_EMAC3_BASE+0x18)
#define	E3_INTR_OVERRUN			(1<<25)	//this bit does NOT WORKED
#define	E3_INTR_PF			(1<<24)
#define	E3_INTR_BAD_FRAME		(1<<23)
#define	E3_INTR_RUNT_FRAME		(1<<22)
#define	E3_INTR_SHORT_EVENT		(1<<21)
#define	E3_INTR_ALIGN_ERR		(1<<20)
#define	E3_INTR_BAD_FCS			(1<<19)
#define	E3_INTR_TOO_LONG		(1<<18)
#define	E3_INTR_OUT_RANGE_ERR		(1<<17)
#define	E3_INTR_IN_RANGE_ERR		(1<<16)
#define	E3_INTR_DEAD_DEPEND		(1<<9)
#define	E3_INTR_DEAD_0			(1<<8)
#define	E3_INTR_SQE_ERR_0		(1<<7)
#define	E3_INTR_TX_ERR_0		(1<<6)
#define	E3_INTR_DEAD_1			(1<<5)
#define	E3_INTR_SQE_ERR_1		(1<<4)
#define	E3_INTR_TX_ERR_1		(1<<3)
#define	E3_INTR_MMAOP_SUCCESS		(1<<1)
#define	E3_INTR_MMAOP_FAIL		(1<<0)
#define	E3_INTR_ALL			(E3_INTR_OVERRUN|E3_INTR_PF|E3_INTR_BAD_FRAME|E3_INTR_RUNT_FRAME|E3_INTR_SHORT_EVENT|	\
					 E3_INTR_ALIGN_ERR|E3_INTR_BAD_FCS|E3_INTR_TOO_LONG|E3_INTR_OUT_RANGE_ERR|		\
					 E3_INTR_IN_RANGE_ERR|E3_INTR_DEAD_DEPEND|E3_INTR_DEAD_0| E3_INTR_SQE_ERR_0|		\
					 E3_INTR_TX_ERR_0|E3_INTR_DEAD_1|E3_INTR_SQE_ERR_1|E3_INTR_TX_ERR_1|			\
					 E3_INTR_MMAOP_SUCCESS|E3_INTR_MMAOP_FAIL)
#define	E3_DEAD_ALL			(E3_INTR_DEAD_DEPEND|E3_INTR_DEAD_0| E3_INTR_DEAD_1)

#define	SMAP_EMAC3_ADDR_HI		(SMAP_EMAC3_BASE+0x1C)
#define	SMAP_EMAC3_ADDR_LO		(SMAP_EMAC3_BASE+0x20)

#define	SMAP_EMAC3_VLAN_TPID		(SMAP_EMAC3_BASE+0x24)
#define	E3_VLAN_ID_MSK			0xFFFF

#define	SMAP_EMAC3_VLAN_TCI		(SMAP_EMAC3_BASE+0x28)
#define	E3_VLAN_TCITAG_MSK		0xFFFF

#define	SMAP_EMAC3_PAUSE_TIMER		(SMAP_EMAC3_BASE+0x2C)
#define	E3_PTIMER_MSK			0xFFFF

#define	SMAP_EMAC3_INDIVID_HASH1	(SMAP_EMAC3_BASE+0x30)
#define	SMAP_EMAC3_INDIVID_HASH2	(SMAP_EMAC3_BASE+0x34)
#define	SMAP_EMAC3_INDIVID_HASH3	(SMAP_EMAC3_BASE+0x38)
#define	SMAP_EMAC3_INDIVID_HASH4	(SMAP_EMAC3_BASE+0x3C)
#define	SMAP_EMAC3_GROUP_HASH1		(SMAP_EMAC3_BASE+0x40)
#define	SMAP_EMAC3_GROUP_HASH2		(SMAP_EMAC3_BASE+0x44)
#define	SMAP_EMAC3_GROUP_HASH3		(SMAP_EMAC3_BASE+0x48)
#define	SMAP_EMAC3_GROUP_HASH4		(SMAP_EMAC3_BASE+0x4C)
#define	E3_HASH_MSK			0xFFFF

#define	SMAP_EMAC3_LAST_SA_HI		(SMAP_EMAC3_BASE+0x50)
#define	SMAP_EMAC3_LAST_SA_LO		(SMAP_EMAC3_BASE+0x54)

#define	SMAP_EMAC3_INTER_FRAME_GAP	(SMAP_EMAC3_BASE+0x58)
#define	E3_IFGAP_MSK			0x3F

#define	SMAP_EMAC3_STA_CTRL		(SMAP_EMAC3_BASE+0x5C)
#define	E3_PHY_DATA_MSK			(0xFFFF)
#define	E3_PHY_DATA_BITSFT		(16)
#define	E3_PHY_OP_COMP			(1<<15)	//operation complete
#define	E3_PHY_ERR_READ			(1<<14)
#define	E3_PHY_STA_CMD_BITSFT		(12)
#define	E3_PHY_READ			(1<<12)
#define	E3_PHY_WRITE			(2<<12)
#define	E3_PHY_OPBCLCK_BITSFT		(10)
#define	E3_PHY_50M			(0<<10)
#define	E3_PHY_66M			(1<<10)
#define	E3_PHY_83M			(2<<10)
#define	E3_PHY_100M			(3<<10)
#define	E3_PHY_ADDR_MSK			(0x1F)
#define	E3_PHY_ADDR_BITSFT		(5)
#define	E3_PHY_REG_ADDR_MSK		(0x1F)

#define	SMAP_EMAC3_TX_THRESHOLD		(SMAP_EMAC3_BASE+0x60)
#define	E3_TX_THRESHLD_MSK		(0x1F)
#define	E3_TX_THRESHLD_BITSFT		(27)

#define	SMAP_EMAC3_RX_WATERMARK		(SMAP_EMAC3_BASE+0x64)
#define	E3_RX_LO_WATER_MSK		(0x1FF)
#define	E3_RX_LO_WATER_BITSFT		(23)
#define	E3_RX_HI_WATER_MSK		(0x1FF)
#define	E3_RX_HI_WATER_BITSFT		(7)

#define	SMAP_EMAC3_TX_OCTETS		(SMAP_EMAC3_BASE+0x68)
#define	SMAP_EMAC3_RX_OCTETS		(SMAP_EMAC3_BASE+0x6C)


struct pbuf;

typedef enum SMapStatus		{SMap_OK,SMap_Err,SMap_Con,SMap_TX} SMapStatus;

//SMap SMap0;

//Function prototypes
int			SMap_Init(void);
void			SMap_SetMacAddress(const char *addr);
void			SMap_Start(void);
void			SMap_Stop(void);
int			SMap_CanSend(void);
SMapStatus		SMap_Send(struct pbuf* pPacket);
int			SMap_HandleTXInterrupt(int iFlags);
int			SMap_HandleRXEMACInterrupt(int iFlags);
u8 const*		SMap_GetMACAddress(void);
void			SMap_EnableInterrupts(int iFlags);
void			SMap_DisableInterrupts(int iFlags);
int			SMap_GetIRQ(void);
void			SMap_ClearIRQ(int iFlags);
void			SMap_SetLink(SMap* pSMap, u32 iLNK, u32 iFD, u32 i10M);


#define	INTR_EMAC3		(1<<6)
#define	INTR_RXEND		(1<<5)
#define	INTR_TXEND		(1<<4)
#define	INTR_RXDNV		(1<<3)	//descriptor not valid
#define	INTR_TXDNV		(1<<2)	//descriptor not valid
//#define	INTR_CLR_ALL		(INTR_RXEND|INTR_TXEND|INTR_RXDNV)
//#define	INTR_ENA_ALL		(INTR_EMAC3|INTR_CLR_ALL)
#define	INTR_ALL		(INTR_EMAC3|INTR_RXEND|INTR_TXEND|INTR_RXDNV|INTR_TXDNV)

#endif	/* __SMAP_H__ */
