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

// Background color debugging
// https://sashamaps.net/docs/resources/20-colors/
// 'convenient' ordering
//#define BGCOLOR_RED      GS_SET_BGCOLOR(230,  25,  75) // ERROR
#define BGCOLOR_RED      GS_SET_BGCOLOR(255,   0,   0) // ERROR
#define BGCOLOR_GREEN    GS_SET_BGCOLOR( 60, 180,  75)
#define BGCOLOR_YELLOW   GS_SET_BGCOLOR(255, 225,  25)
#define BGCOLOR_BLUE     GS_SET_BGCOLOR(  0, 130, 200) // IOP reboot
#define BGCOLOR_ORANGE   GS_SET_BGCOLOR(245, 130,  48)
#define BGCOLOR_PURPLE   GS_SET_BGCOLOR(145,  30, 180) // IOP reboot
#define BGCOLOR_CYAN     GS_SET_BGCOLOR( 70, 240, 240) // IOP reboot
#define BGCOLOR_MAGENTA  GS_SET_BGCOLOR(240,  50, 230) // IOP reboot
#define BGCOLOR_LIME     GS_SET_BGCOLOR(210, 245,  60)
#define BGCOLOR_PINK     GS_SET_BGCOLOR(250, 190, 212)
#define BGCOLOR_TEAL     GS_SET_BGCOLOR(  0, 128, 128)
#define BGCOLOR_LAVENDER GS_SET_BGCOLOR(220, 190, 255)
#define BGCOLOR_BROWN    GS_SET_BGCOLOR(170, 110,  40)
#define BGCOLOR_BEIGE    GS_SET_BGCOLOR(255, 250, 200)
#define BGCOLOR_MAROON   GS_SET_BGCOLOR(128,   0,   0)
#define BGCOLOR_MINT     GS_SET_BGCOLOR(170, 255, 195)
#define BGCOLOR_OLIVE    GS_SET_BGCOLOR(128, 128,   0)
#define BGCOLOR_APRICOT  GS_SET_BGCOLOR(255, 215, 180)
#define BGCOLOR_NAVY     GS_SET_BGCOLOR(  0,   0, 128)
#define BGCOLOR_GREY     GS_SET_BGCOLOR(128, 128, 128)
#define BGCOLOR_WHITE    GS_SET_BGCOLOR(255, 255, 255)
#define BGCOLOR_BLACK    GS_SET_BGCOLOR(  0,   0,   0)


#endif
