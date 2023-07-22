#ifndef MODULES_H
#define MODULES_H

enum EECORE_MODULE_ID {
    EECORE_MODULE_ID_IOPRP = 1,
    EECORE_MODULE_ID_CDVDMAN,
    EECORE_MODULE_ID_CDVDFSV,
    EECORE_MODULE_ID_EESYNC,
    EECORE_MODULE_ID_IMGDRV,
    EECORE_MODULE_ID_USBD,
    EECORE_MODULE_ID_IOP_PATCH,
};

typedef struct
{
    void *ptr;
    unsigned int info; // Upper 8 bits = module ID
} irxptr_t;

typedef struct
{
    irxptr_t *modules;
    int count;
} irxtab_t;

// Macros for working with module information.
#define GET_OPL_MOD_ID(x)   ((x) >> 24)
#define SET_OPL_MOD_ID(x)   ((x) << 24)
#define GET_OPL_MOD_SIZE(x) ((x)&0x00FFFFFF)

#endif
