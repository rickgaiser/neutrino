#ifndef IOPMGR_H
#define IOPMGR_H


void services_start(void);
void services_exit(void);

void New_Reset_Iop(const char *arg, int arglen);
void New_Reset_Iop2(const char *arg, int arglen, int reboot_mode, int force);

void Install_Kernel_Hooks(void);


#endif
