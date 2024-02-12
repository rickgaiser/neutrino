
#ifndef __CDVDMAN_INTERNAL__
#define __CDVDMAN_INTERNAL__

#include "dev9.h"
#include "cdvdman_opl.h"
#include "cdvd_config.h"
#include "device.h"

#include <loadcore.h>
#include <stdio.h>
#include <sifman.h>
#include <sysclib.h>
#include <sysmem.h>
#include <thbase.h>
#include <thevent.h>
#include <intrman.h>
#include <ioman.h>
#include <thsemap.h>
#include <errno.h>
#include <io_common.h>
#include <cdvdman.h>
#include "ioman_add.h"

#include <defs.h>
#include "mprintf.h"

#define MODNAME "cdvd_driver"

// Event flags
#define CDVDEF_MAN_UNLOCKED  0x0001
#define CDVDEF_POWER_OFF     0x0002
#define CDVDEF_FSV_S596      0x0004
#define CDVDEF_STM_DONE      0x0008 // Streaming read done
#define CDVDEF_READ_END      0x1000 // Accurate reads timing event
#define CDVDEF_CB_DONE       0x2000

struct SteamingData
{
    unsigned short int StBufmax;
    unsigned short int StBankmax;
    unsigned short int StBanksize;
    unsigned short int StWritePtr;
    unsigned short int StReadPtr;
    unsigned short int StStreamed;
    unsigned short int StStat;
    unsigned short int StIsReading;
    void *StIOP_bufaddr;
    u32 Stlsn;
};

enum ECallSource {
    ECS_EXTERNAL = 0,
    ECS_SEARCHFILE,
    ECS_STREAMING,
    ECS_EE_RPC
};

typedef struct
{
    u32 lba;
    u32 sectors;
    u16 sector_size;
    void *buf;
    enum ECallSource source;
} cdvdman_read_t;

typedef struct
{
    int err;
    u8 status; // SCECdvdDriveState
    struct SteamingData StreamingData;
    int intr_ef;
    int disc_type_reg; // SCECdvdMediaType
    cdvdman_read_t req; // Next requested read
} cdvdman_status_t;

struct dirTocEntry
{
    short length;
    u32 fileLBA;         // 2
    u32 fileLBA_bigend;  // 6
    u32 fileSize;        // 10
    u32 fileSize_bigend; // 14
    u8 dateStamp[6];     // 18
    u8 reserved1;        // 24
    u8 fileProperties;   // 25
    u8 reserved2[6];     // 26
    u8 filenameLength;   // 32
    char filename[128];  // 33
} __attribute__((packed));

typedef void (*StmCallback_t)(void);

// Internal (common) function prototypes
extern void SetStm0Callback(StmCallback_t callback);

extern int sceCdRead_internal(u32 lsn, u32 sectors, void *buf, sceCdRMode *mode, enum ECallSource source);
extern int sceCdGetToc_internal(u8 *toc, enum ECallSource source);
extern int sceCdSeek_internal(u32 lsn, enum ECallSource source);
extern int sceCdStandby_internal(enum ECallSource source);
extern int sceCdStop_internal(enum ECallSource source);
extern int sceCdPause_internal(enum ECallSource source);
extern int sceCdBreak_internal(enum ECallSource source);

extern int cdvdman_sendSCmd(u8 cmd, const void *in, u16 in_size, void *out, u16 out_size);
extern void cdvdman_cb_event(int reason);

extern void cdvdman_init(void);
extern void cdvdman_fs_init(void);
extern void cdvdman_searchfile_init(void);
extern void cdvdman_initdev(void);

extern struct cdvdman_settings_common cdvdman_settings;

// Normally this buffer is only used by 'searchfile', only 1 sector used
#define CDVDMAN_BUF_SECTORS 1
extern u8 cdvdman_buf[CDVDMAN_BUF_SECTORS * 2048];
#define CDVDMAN_FS_BUF_ALIGNMENT 64
extern u8 *cdvdman_fs_buf;

extern int cdrom_io_sema;
extern int cdvdman_searchfilesema;

extern cdvdman_status_t cdvdman_stat;

extern volatile unsigned char sync_flag_locked;
extern volatile unsigned char cdvdman_cdinited;
extern u32 mediaLsnCount;

#endif
