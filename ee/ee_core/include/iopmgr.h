#ifndef IOPMGR_H
#define IOPMGR_H


void services_start(void);
void services_exit(void);

// Game-safe raw IOP reset used by IGR. Unlike PS2SDK's SifIopReset, this does
// not stop SIF0 while a title may still be transmitting to the EE.
int Reset_Iop(const char *arg, int mode);
void New_Reset_Iop(const char *arg, int arglen);
void New_Reset_Iop2(const char *arg, int arglen, int eeload);

void Install_Kernel_Hooks(void);
void Remove_Kernel_Hooks(void);


#endif
