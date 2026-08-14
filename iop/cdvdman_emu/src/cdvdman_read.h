#ifndef CDVDMAN_READ_H
#define CDVDMAN_READ_H


#include <tamtypes.h>


typedef void (*StmCallback_t)(void);


extern volatile unsigned char sync_flag_locked;

// Prevent new reads and wait for the current backend request to complete.
// The lock intentionally remains held until the imminent IOP reset.
void cdvdman_shutdown_io(void);


void cdvdman_read_init();

int sceCdRead_internal(u32 lsn, u32 sectors, void *buf, sceCdRMode *mode, enum ECallSource source);
StmCallback_t cdvdman_read_set_stm0_callback(StmCallback_t callback);


#endif
