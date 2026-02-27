#include <loadcore.h>
#include <irx.h>

#include "main.h"

#ifdef FEATURE_UDPFS_BD
int udpfs_bd_init(void);
#endif
#ifdef FEATURE_UDPFS_IOMAN
int udpfs_init(void);
#endif
#ifdef FEATURE_UDPFS_FHI
int fhi_init(void);
#endif

IRX_ID(MODNAME, 0x2, 0x1A);

#ifdef FEATURE_UDPFS_FHI
extern struct irx_export_table _exp_fhi __attribute__((section("data")));
#endif

int _start(int argc, char *argv[])
{
    (void)argc; (void)argv;

    /* Start udpfs as local IOP file system: "udpfs:" */
#ifdef FEATURE_UDPFS_IOMAN
    udpfs_init();
#endif

    /* Start udpfs as BDM block device */
#ifdef FEATURE_UDPFS_BD
    udpfs_bd_init();
#endif

    /* Start udpfs FHI interface */
#ifdef FEATURE_UDPFS_FHI
    if (RegisterLibraryEntries(&_exp_fhi) != 0) {
        M_DEBUG("udpfs: module already loaded\n");
        return MODULE_NO_RESIDENT_END;
    }
    fhi_init();
#endif

    return MODULE_RESIDENT_END;
}
