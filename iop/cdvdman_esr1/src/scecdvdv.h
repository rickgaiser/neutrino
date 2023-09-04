#ifndef __CDVD_DEFS_H
#define __CDVD_DEFS_H

enum outType {
    enum_def_sceCdCallback = 0,
    enum_hook_sceCdCallback,
    enum_def_sceCdstm0Cb,
    enum_hook_sceCdstm0Cb,
    enum_def_sceCdstm1Cb,
    enum_hook_sceCdstm1Cb,
    enum_def_sceCdRead0,
    enum_hook_sceCdRead0,
    enum_def_sceCdMmode,
    enum_hook_sceCdMmode,
    enum_discType,
    enum_def_RegisterIntrHandler,
    enum_hook_RegisterIntrHandler,
    enum_def_AllocSysMemory,
    enum_hook_AllocSysMemory
};

void readCallback(int reason);

#endif //__CDVD_DEFS_H
