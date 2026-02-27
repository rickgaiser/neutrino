#ifndef MPRINTF_H
#define MPRINTF_H


#include <stdio.h>

#ifdef DEBUG
#define DEBUG_VAL 1
#else
#define DEBUG_VAL 0
#endif

#define M_PRINTF(fmt, ...) \
    printf(MODNAME ": " fmt, ##__VA_ARGS__)

#define M_DEBUG(fmt, ...) \
    do { if (DEBUG_VAL) M_PRINTF(fmt, ##__VA_ARGS__); } while (0)


#endif
