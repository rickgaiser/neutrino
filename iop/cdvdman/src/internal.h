
#ifndef __CDVDMAN_INTERNAL__
#define __CDVDMAN_INTERNAL__

#include "dev9.h"
#include "ioplib_util.h"
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

#include "smsutils.h"

#ifdef DEBUG
#define DPRINTF(args...)  printf(args)
#define iDPRINTF(args...) Kprintf(args)
#else
#define DPRINTF(args...)
#define iDPRINTF(args...)
#endif

#define CDVDMAN_SETTINGS_DEFAULT_COMMON         \
    {                                           \
        0x69, 0x69, 0x1234, 0x39393939, "B00BS" \
    }

#ifdef BDM_DRIVER
#define CDVDMAN_SETTINGS_TYPE cdvdman_settings_bdm
#define CDVDMAN_SETTINGS_DEFAULT_DEVICE_SETTINGS
#elif defined FILE_DRIVER
#define CDVDMAN_SETTINGS_TYPE cdvdman_settings_file
#define CDVDMAN_SETTINGS_DEFAULT_DEVICE_SETTINGS
#else
#error Unknown driver type. Please check the Makefile.
#endif

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
    void *buf;
    enum ECallSource source;
} cdvdman_read_t;

typedef struct
{
    int err;
    int status;
    struct SteamingData StreamingData;
    int intr_ef;
    int disc_type_reg;
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

extern struct CDVDMAN_SETTINGS_TYPE cdvdman_settings;

extern int cdrom_io_sema;
extern int cdvdman_searchfilesema;

extern cdvdman_status_t cdvdman_stat;

extern volatile unsigned char sync_flag_locked;
extern volatile unsigned char cdvdman_cdinited;
extern u32 mediaLsnCount;

#endif
