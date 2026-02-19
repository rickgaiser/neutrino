#include <errno.h>
#include <stdio.h>
#include <loadcore.h>
#include <thbase.h>
#include <irx.h>
#include <sysclib.h>

#include "main.h"
#include "xfer.h"
#include "ministack_ip.h"

#ifdef FEATURE_UDPTTY
#include "udptty.h"
#endif
#ifdef FEATURE_UDPFS_BD
int udpfs_bd_init(void);
#endif
#ifdef FEATURE_UDPFS_IOMAN
int udpfs_init(void);
#endif
#ifdef FEATURE_UDPFS_FHI
int fhi_init(void);
#endif

// Last SDK 3.1.0 has INET family version "2.26.0"
// SMAP module is the same as "2.25.0"
IRX_ID(MODNAME, 0x2, 0x1A);

#ifdef FEATURE_UDPFS_FHI
extern struct irx_export_table _exp_fhi __attribute__((section("data")));
#endif

uint32_t parse_ip(const char *sIP)
{
    int cp = 0;
    uint32_t part[4] = {0,0,0,0};

    while(*sIP != 0) {
        if(*sIP == '.') {
            cp++;
            if (cp >= 4)
                return 0; // Too many dots
        } else if(*sIP >= '0' && *sIP <= '9') {
            part[cp] = (part[cp] * 10) + (*sIP - '0');
            if (part[cp] > 255)
                return 0; // Too big number
        } else {
            return 0; // Invalid character
        }
        sIP++;
    }

    if (cp != 3)
        return 0; // Too little dots

    return IP_ADDR((uint8_t)part[0], (uint8_t)part[1], (uint8_t)part[2], (uint8_t)part[3]);
}

int _start(int argc, char *argv[])
{
    int result;
    int i;

    // Initialize SMAP
    if ((result = smap_init(argc, argv)) < 0) {
        M_DEBUG("smap: smap_init -> %d\n", result);
        return MODULE_NO_RESIDENT_END;
    }

    // Parse command line IP address
    for (i=1; i<argc; i++) {
        M_DEBUG("argv[%d] = %s\n", i, argv[i]);
        if (!strncmp(argv[i], "ip=", 3)) {
            uint32_t ip = parse_ip(&argv[i][3]);
            if (ip != 0)
                ms_ip_set_ip(ip);
        }
    }

    // Start UDPTTY debugging
#ifdef FEATURE_UDPTTY
    udptty_init();
#endif

    // Start udpfs as local IOP file system: "udpfs:"
#ifdef FEATURE_UDPFS_IOMAN
    udpfs_init();
#endif

    // Start udpfs as BDM block device
#ifdef FEATURE_UDPFS_BD
    udpfs_bd_init();
#endif

    // Start udpfs FHI interface
#ifdef FEATURE_UDPFS_FHI
    if (RegisterLibraryEntries(&_exp_fhi) != 0) {
        M_DEBUG("smap: module already loaded\n");
        return MODULE_NO_RESIDENT_END;
    }
    fhi_init();
#endif

    return MODULE_RESIDENT_END;
}
