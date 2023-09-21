#include <errno.h>
#include <stdio.h>
#include <loadcore.h>
#include <thbase.h>
#include <irx.h>
#include <sysclib.h>

#include "main.h"
#include "xfer.h"
#include "ministack.h"

// Last SDK 3.1.0 has INET family version "2.26.0"
// SMAP module is the same as "2.25.0"
IRX_ID(MODNAME, 0x2, 0x1A);

//While the header of the export table is small, the large size of the export table (as a whole) places it in data instead of sdata.
extern struct irx_export_table _exp_smap __attribute__((section("data")));

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

    if (RegisterLibraryEntries(&_exp_smap) != 0) {
        M_DEBUG("smap: module already loaded\n");
        return MODULE_NO_RESIDENT_END;
    }

    if ((result = smap_init(argc, argv)) < 0) {
        M_DEBUG("smap: smap_init -> %d\n", result);
        ReleaseLibraryEntries(&_exp_smap);
        return MODULE_NO_RESIDENT_END;
    }

    for (i=1; i<argc; i++) {
        M_DEBUG("argv[%d] = %s\n", i, argv[i]);
        if (!strncmp(argv[i], "ip=", 3)) {
            uint32_t ip = parse_ip(&argv[i][3]);
            if (ip != 0)
                ms_ip_set_ip(ip);
        }
    }

    return MODULE_RESIDENT_END;
}
