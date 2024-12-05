#ifndef CDVDMAN_READ_H
#define CDVDMAN_READ_H


#include <tamtypes.h>


typedef void (*StmCallback_t)(void);


extern volatile unsigned char sync_flag_locked;


void cdvdman_read_init();

int sceCdRead_internal(u32 lsn, u32 sectors, void *buf, sceCdRMode *mode, enum ECallSource source);
void cdvdman_read_set_stm0_callback(StmCallback_t callback);


#endif
