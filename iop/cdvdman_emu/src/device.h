void DeviceFSInit(void);  // Called when the filesystem layer is initialized
int DeviceReadSectors(u32 lsn, void *buffer, unsigned int sectors);
