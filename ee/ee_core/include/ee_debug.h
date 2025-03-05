#ifndef EE_DEBUG_H
#define EE_DEBUG_H


#include <kernel.h>
#include <gs_privileged.h> // BGCOLOR


// PRINTF debugging
#ifdef __EESIO_DEBUG
#define DPRINTF(args...) _print(args)
#define DINIT()          InitDebug()
#else
#define DPRINTF(args...) \
    do {                 \
    } while (0)
#define DINIT() \
    do {        \
    } while (0)
#endif

#define GSCOLOR32(R, G, B) ( \
    (u32)((R)&0x000000FF) <<  0 | \
    (u32)((G)&0x000000FF) <<  8 | \
    (u32)((B)&0x000000FF) << 16)

// Background color debugging
// BW
#define COLOR_BLACK    GSCOLOR32(  0,   0,   0)
#define COLOR_WHITE    GSCOLOR32(255, 255, 255)
// RGB (1x255)
#define COLOR_RED      GSCOLOR32(255,   0,   0) // ERROR
#define COLOR_GREEN    GSCOLOR32(  0, 255,   0)
#define COLOR_BLUE     GSCOLOR32(  0,   0, 255)
// 2x255
#define COLOR_LBLUE    GSCOLOR32(  0, 255, 255) // Before IOP reboot 1
#define COLOR_MAGENTA  GSCOLOR32(255,   0, 255) // Before IOP reboot 2
#define COLOR_YELLOW   GSCOLOR32(255, 255,   0) // Before IOP reboot 3
// 2x128
#define COLOR_TEAL     GSCOLOR32(  0, 128, 128)
#define COLOR_PURPLE   GSCOLOR32(128,   0, 128) // IOP reboot errors
#define COLOR_OLIVE    GSCOLOR32(128, 128,   0) // GSM errors

// Colors of function error codes
#define COLOR_FUNC_IOPREBOOT COLOR_PURPLE
#define COLOR_FUNC_GSM       COLOR_OLIVE


#endif
