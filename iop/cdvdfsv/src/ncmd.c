/*
  Copyright 2009, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review open-ps2-loader README & LICENSE files for further details.
*/

#include "cdvdfsv-internal.h"

typedef struct
{
    u32 lsn;
    u32 sectors;
    void *buf;
    sceCdRMode mode;
    void *eeaddr1;
    void *eeaddr2;
} RpcCdvd_t;

typedef struct
{
    u32 lsn;
    u32 sectors;
    void *buf;
    int cmd;
    sceCdRMode mode;
    u32 pad;
} RpcCdvdStream_t;

typedef struct
{
    u32 bufmax;
    u32 bankmax;
    void *buf;
    u32 pad;
} RpcCdvdStInit_t;

typedef struct
{
    u32 lsn;      // sector location to start reading from
    u32 sectors;  // number of sectors to read
    void *buf;    // buffer address to read to ( bit0: 0=EE, 1=IOP )
} RpcCdvdchain_t; // (EE addresses must be on 64byte alignment)

typedef struct
{ // size = 144
    u32 b1len;
    u32 b2len;
    void *pdst1;
    void *pdst2;
    u8 buf1[64];
    u8 buf2[64];
} cdvdfsv_readee_t;

static sceCdRMode cdvdfsv_Stmode;

static SifRpcServerData_t cdvdNcmds_rpcSD;
static u8 cdvdNcmds_rpcbuf[1024];

static void *cbrpc_cdvdNcmds(int fno, void *buf, int size);
static inline int cdvd_readee(RpcCdvd_t *r);
static inline int cdvdSt_read(RpcCdvdStream_t *St);
static inline int cdvd_Stsubcmdcall(void *buf);
static inline int cdvd_readiopm(RpcCdvd_t *r);
static inline int cdvd_readchain(RpcCdvdchain_t *ch);
static inline int rpcNCmd_cdreadDiskID(void *buf);
static inline int rpcNCmd_cdgetdisktype(void *buf);

enum CD_NCMD_CMDS {
    CD_NCMD_READ = 1,
    CD_NCMD_CDDAREAD,
    CD_NCMD_DVDREAD,
    CD_NCMD_GETTOC,
    CD_NCMD_SEEK,
    CD_NCMD_STANDBY,
    CD_NCMD_STOP,
    CD_NCMD_PAUSE,
    CD_NCMD_STREAM,
    CD_NCMD_CDDASTREAM,
    CD_NCMD_NCMD = 0x0C,
    CD_NCMD_READIOPMEM,
    CD_NCMD_DISKREADY,
    CD_NCMD_READCHAIN,
    CD_NCMD_READDISKID = 0x11,
    CD_NCMD_DISKTYPE = 0x17,

    CD_NCMD_COUNT
};

enum CDVD_ST_CMDS {
    CDVD_ST_CMD_START = 1,
    CDVD_ST_CMD_READ,
    CDVD_ST_CMD_STOP,
    CDVD_ST_CMD_SEEK,
    CDVD_ST_CMD_INIT,
    CDVD_ST_CMD_STAT,
    CDVD_ST_CMD_PAUSE,
    CDVD_ST_CMD_RESUME,
    CDVD_ST_CMD_SEEKF
};

//#define READ_EE_SIMPLE
#ifdef READ_EE_SIMPLE
//--------------------------------------------------------------
static inline int cdvd_readee(RpcCdvd_t *r)
{
    u32 sectors_to_read, bytesent;

    //M_DEBUG("%s %d %d @0x%X -> 0x%X / 0x%X)\n", __FUNCTION__, r->lsn, r->sectors, r->buf, r->eeaddr1, r->eeaddr2);

    // Sanity check
    if (r->sectors == 0)
        return 0;

    // 64b alignment check
    if ((u32)r->buf & 0x3f) {
        M_DEBUG("%s: not aligned!\n", __FUNCTION__);
        return 0;
    }

    r->eeaddr1 = (void *)((u32)r->eeaddr1 & 0x1fffffff);
    r->eeaddr2 = (void *)((u32)r->eeaddr2 & 0x1fffffff);
    r->buf = (void *)((u32)r->buf & 0x1fffffff);

    sceCdDiskReady(0);

    bytesent = 0;
    sectors_to_read = r->sectors;
    while (sectors_to_read) {
        u32 nsectors = (sectors_to_read < cdvdfsv_sectors) ? sectors_to_read : cdvdfsv_sectors;

        if (sceCdGetError() == SCECdErABRT)
            break;

        // Read sectors
        while (sceCdRead(r->lsn, nsectors, cdvdfsv_buf, NULL) == 0)
            DelayThread(10000);

        // Wait for read to complete
        sceCdSync(0);

        // Transfer data EE
        sysmemSendEE(cdvdfsv_buf, r->buf, nsectors * 2048);

        // Update statistics
        sectors_to_read -= nsectors;
        r->lsn          += nsectors;
        r->buf          += nsectors * 2048;
        bytesent        += nsectors * 2048;
    }

    {
        cdvdfsv_readee_t readee;
        readee.pdst1 = r->buf;
        readee.b1len = 0;
        readee.pdst2 = (u8*)r->buf + bytesent;
        readee.b2len = 0;

        u8 curlsn_buf[16];
        memset((void *)curlsn_buf, 0, 16);
        *((u32 *)&curlsn_buf[0]) = bytesent;

        //sysmemSendEE(&readee, r->eeaddr1, sizeof(cdvdfsv_readee_t));
        //sysmemSendEE((void *)curlsn_buf, r->eeaddr2, 16);
        sysmemSendEE2(&readee, r->eeaddr1, sizeof(cdvdfsv_readee_t), (void *)curlsn_buf, r->eeaddr2, 16);
    }

    return bytesent;
}
#else
//--------------------------------------------------------------
static inline int cdvd_readee(RpcCdvd_t *r)
{ // Read Disc data to EE mem buffer
    u8 curlsn_buf[16];
    u32 nbytes, nsectors, sectors_to_read, size_64b, size_64bb, bytesent, temp;
    u16 sector_size;
    int flag_64b, fsverror;
    void *fsvRbuf = (void *)cdvdfsv_buf;
    void *eeaddr_64b, *eeaddr2_64b;
    cdvdfsv_readee_t readee;

    //M_DEBUG("%s %d %d @0x%X -> 0x%X / 0x%X)\n", __FUNCTION__, r->lsn, r->sectors, r->buf, r->eeaddr1, r->eeaddr2);

    if (r->sectors == 0)
        return 0;

    sector_size = 2048;

    if (r->mode.datapattern == SCECdSecS2328)
        sector_size = 2328;
    if (r->mode.datapattern == SCECdSecS2340)
        sector_size = 2340;

    r->eeaddr1 = (void *)((u32)r->eeaddr1 & 0x1fffffff);
    r->eeaddr2 = (void *)((u32)r->eeaddr2 & 0x1fffffff);
    r->buf = (void *)((u32)r->buf & 0x1fffffff);

    sceCdDiskReady(0);

    sectors_to_read = r->sectors;
    bytesent = 0;

    memset((void *)curlsn_buf, 0, 16);

    readee.pdst1 = (void *)r->buf;
    eeaddr_64b = (void *)(((u32)r->buf + 0x3f) & 0xffffffc0); // get the next 64-bytes aligned address

    if ((u32)r->buf & 0x3f)
        readee.b1len = (((u32)r->buf & 0xffffffc0) - (u32)r->buf) + 64; // get how many bytes needed to get a 64 bytes alignment
    else
        readee.b1len = 0;

    nbytes = r->sectors * sector_size;

    temp = (u32)r->buf + nbytes;
    eeaddr2_64b = (void *)(temp & 0xffffffc0);
    temp -= (u32)eeaddr2_64b;
    readee.pdst2 = eeaddr2_64b; // get the end address on a 64 bytes align
    readee.b2len = temp;        // get bytes remainder at end of 64 bytes align
    fsvRbuf += temp;

    if (readee.b1len)
        flag_64b = 0; // 64 bytes alignment flag
    else {
        if (temp)
            flag_64b = 0;
        else
            flag_64b = 1;
    }

    while (1) {
        do {
            if ((sectors_to_read == 0) || (sceCdGetError() == SCECdErABRT)) {
                *((u32 *)&curlsn_buf[0]) = nbytes;
                sysmemSendEE2(&readee, r->eeaddr1, sizeof(cdvdfsv_readee_t), (void *)curlsn_buf, r->eeaddr2, 16);
                return nbytes;
            }

            if (flag_64b == 0) { // not 64 bytes aligned buf
                // The data of the last sector of the chunk will be used to correct buffer alignment.
                if (sectors_to_read < cdvdfsv_sectors - 1)
                    nsectors = sectors_to_read;
                else
                    nsectors = cdvdfsv_sectors - 1;
                temp = nsectors + 1;
            } else { // 64 bytes aligned buf
                if (sectors_to_read < cdvdfsv_sectors)
                    nsectors = sectors_to_read;
                else
                    nsectors = cdvdfsv_sectors;
                temp = nsectors;
            }

            if (sceCdRead(r->lsn, temp, (void *)fsvRbuf, NULL) == 0) {
                if (sceCdGetError() == SCECdErNO) {
                    fsverror = SCECdErREADCF;
                    sceCdSC(CDSC_SET_ERROR, &fsverror);
                }

                return bytesent;
            }
            sceCdSync(0);

            size_64b = nsectors * sector_size;
            size_64bb = size_64b;

            if (!flag_64b) {
                if (sectors_to_read == r->sectors) // check that was the first read. Data read will be skewed by readee.b1len bytes into the adjacent sector.
                    memcpy((void *)readee.buf1, (void *)fsvRbuf, readee.b1len);

                if ((sectors_to_read == nsectors) && (readee.b1len)) // For the last sector read.
                    size_64bb = size_64b - 64;
            }

            if (size_64bb > 0) {
                bytesent += size_64bb;
                *((u32 *)&curlsn_buf[0]) = bytesent;
                sysmemSendEE2((void *)(fsvRbuf + readee.b1len), eeaddr_64b, size_64bb, (void *)curlsn_buf, r->eeaddr2, 16);
            } else {
                *((u32 *)&curlsn_buf[0]) = bytesent;
                sysmemSendEE((void *)curlsn_buf, r->eeaddr2, 16);
            }

            sectors_to_read -= nsectors;
            r->lsn += nsectors;
            eeaddr_64b += size_64b;

        } while ((flag_64b) || (sectors_to_read));

        // At the very last pass, copy readee.b2len bytes from the last sector, to complete the alignment correction.
        if (readee.b2len)
            memcpy((void *)readee.buf2, (void *)(fsvRbuf + size_64b - readee.b2len), readee.b2len);
    }

    return bytesent;
}
#endif
//-------------------------------------------------------------------------
static inline int cdvdSt_read(RpcCdvdStream_t *St)
{
    u32 err;
    int r, rpos, remaining;
    void *ee_addr;

    //M_DEBUG("%s\n", __FUNCTION__);

    for (rpos = 0, ee_addr = St->buf, remaining = St->sectors; remaining > 0; ee_addr += r * 2048, rpos += r, remaining -= r) {
        if ((r = sceCdStRead(remaining, (void *)((u32)ee_addr | 0x80000000), 0, &err)) < 1)
            break;
    }

    return (rpos & 0xFFFF) | (err << 16);
}

//-------------------------------------------------------------------------
static inline int cdvd_Stsubcmdcall(void *buf)
{ // call a Stream Sub function (below) depending on stream cmd sent
    RpcCdvdStream_t *St = (RpcCdvdStream_t *)buf;
    RpcCdvdStInit_t *Si = (RpcCdvdStInit_t *)buf;

    M_DEBUG("%s cmd=%d\n", __FUNCTION__, St->cmd);

    switch (St->cmd) {
        case CDVD_ST_CMD_START:  return sceCdStStart(St->lsn, &cdvdfsv_Stmode);
        case CDVD_ST_CMD_READ:   return cdvdSt_read(St);
        case CDVD_ST_CMD_STOP:   return sceCdStStop();
        case CDVD_ST_CMD_SEEK:   return sceCdStSeek(St->lsn);
        case CDVD_ST_CMD_INIT:   return sceCdStInit(Si->bufmax, Si->bankmax, Si->buf);
        case CDVD_ST_CMD_STAT:   return sceCdStStat();
        case CDVD_ST_CMD_PAUSE:  return sceCdStPause();
        case CDVD_ST_CMD_RESUME: return sceCdStResume();
        case CDVD_ST_CMD_SEEKF:  return sceCdStSeekF(St->lsn);
    };

    return 0;
}

static inline int cdvd_readiopm(RpcCdvd_t *r)
{
    int rv, fsverror;
    u32 readpos;

    M_DEBUG("%s\n", __FUNCTION__);

    rv = sceCdRead(r->lsn, r->sectors, r->buf, NULL);
    while (sceCdSync(1)) {
        readpos = sceCdGetReadPos();
        sysmemSendEE(&readpos, r->eeaddr2, sizeof(readpos));
        DelayThread(8000);
    }

    if (rv == 0) {
        if (sceCdGetError() == SCECdErNO) {
            fsverror = SCECdErREADCFR;
            sceCdSC(CDSC_SET_ERROR, &fsverror);
        }
    }

    return rv;
}

//-------------------------------------------------------------------------
static inline int cdvd_readchain(RpcCdvdchain_t *ch)
{
    int i, fsverror;
    u32 nsectors, tsectors, lsn, addr, readpos;

    M_DEBUG("%s\n", __FUNCTION__);

    for (i = 0, readpos = 0; i < 64; i++, ch++) {

        if ((ch->lsn == -1) || (ch->sectors == -1) || ((u32)ch->buf == -1))
            break;

        lsn = ch->lsn;
        tsectors = ch->sectors;
        addr = (u32)ch->buf & 0xfffffffc;

        if ((u32)ch->buf & 1) { // IOP addr
            if (sceCdRead(lsn, tsectors, (void *)addr, NULL) == 0) {
                if (sceCdGetError() == SCECdErNO) {
                    fsverror = SCECdErREADCFR;
                    sceCdSC(CDSC_SET_ERROR, &fsverror);
                }

                return 0;
            }
            sceCdSync(0);

            readpos += tsectors * 2048;
        } else { // EE addr
            while (tsectors > 0) {
                nsectors = (tsectors > cdvdfsv_sectors) ? cdvdfsv_sectors : tsectors;

                if (sceCdRead(lsn, nsectors, cdvdfsv_buf, NULL) == 0) {
                    if (sceCdGetError() == SCECdErNO) {
                        fsverror = SCECdErREADCF;
                        sceCdSC(CDSC_SET_ERROR, &fsverror);
                    }

                    return 0;
                }
                sceCdSync(0);
                sysmemSendEE(cdvdfsv_buf, (void *)addr, nsectors * 2048);

                lsn += nsectors;
                tsectors -= nsectors;
                addr += nsectors * 2048;
                readpos += nsectors * 2048;
            }
        }

        // The pointer to the read position variable within EE RAM is stored at ((RpcCdvdchain_t *)buf)[65].sectors.
        sysmemSendEE(&readpos, (void *)ch[65].sectors, sizeof(readpos));
    }

    return 1;
}

//-------------------------------------------------------------------------
static inline int rpcNCmd_cdreadDiskID(void *buf)
{
    M_DEBUG("%s\n", __FUNCTION__);

    u8 *p = (u8 *)buf;
    memset(p, 0, 10);
    return sceCdReadDiskID((unsigned int *)&p[4]);
}

//-------------------------------------------------------------------------
static inline int rpcNCmd_cdgetdisktype(void *buf)
{
    M_DEBUG("%s\n", __FUNCTION__);

    u8 *p = (u8 *)buf;
    *(int *)&p[4] = sceCdGetDiskType();
    return 1;
}

//-------------------------------------------------------------------------
static void *cbrpc_cdvdNcmds(int fno, void *buf, int size)
{ // CD NCMD RPC callback
    int sc_param;
    int result = 0; // error

    M_DEBUG("%s(%d, 0x%X, %d)\n", __FUNCTION__, fno, buf, size);

    sceCdSC(CDSC_IO_SEMA, &fno);

    switch (fno) {
        case CD_NCMD_READ:
        case CD_NCMD_CDDAREAD:
        case CD_NCMD_DVDREAD:
            result = cdvd_readee((RpcCdvd_t *)buf);
            break;
        case CD_NCMD_GETTOC:
            u32 eeaddr = *(u32 *)buf;
            M_DEBUG("cbrpc_cdvdNcmds GetToc eeaddr=%08x\n", (int)eeaddr);
            u8 toc[2064];
            memset(toc, 0, 2064);
            result = sceCdGetToc(toc);
            if (result)
                sysmemSendEE(toc, (void *)eeaddr, 2064);
            break;
        case CD_NCMD_SEEK:
            result = sceCdSeek(*(u32 *)buf);
            break;
        case CD_NCMD_STANDBY:
            result = sceCdStandby();
            break;
        case CD_NCMD_STOP:
            result = sceCdStop();
            break;
        case CD_NCMD_PAUSE:
            result = sceCdPause();
            break;
        case CD_NCMD_STREAM:
            result = cdvd_Stsubcmdcall(buf);
            break;
        case CD_NCMD_READIOPMEM:
            result = cdvd_readiopm((RpcCdvd_t *)buf);
            break;
        case CD_NCMD_DISKREADY:
            result = sceCdDiskReady(0);
            break;
        case CD_NCMD_READCHAIN:
            result = cdvd_readchain((RpcCdvdchain_t *)buf);
            break;
        case CD_NCMD_READDISKID:
            result = rpcNCmd_cdreadDiskID(buf);
            break;
        case CD_NCMD_DISKTYPE:
            result = rpcNCmd_cdgetdisktype(buf);
            break;
        default:
            M_DEBUG("%s(%d, 0x%X, %d) unknown fno\n", __FUNCTION__, fno, buf, size);
            break;
    }

    sc_param = 0;
    sceCdSC(CDSC_IO_SEMA, &sc_param);

    *(int *)buf = result;
    return buf;
}

void cdvdfsv_register_ncmd_rpc(SifRpcDataQueue_t *rpc_DQ)
{
    sceSifRegisterRpc(&cdvdNcmds_rpcSD, 0x80000595, &cbrpc_cdvdNcmds, cdvdNcmds_rpcbuf, NULL, NULL, rpc_DQ);
}
