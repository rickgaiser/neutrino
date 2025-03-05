#ifndef IOPMGR_H
#define IOPMGR_H


void services_start(void);
void services_exit(void);

void New_Reset_Iop(const char *arg, int arglen);

void Install_Kernel_Hooks(void);


#endif
