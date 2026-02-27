#ifndef MSTACK_H
#define MSTACK_H

#include <irx.h>
#include "ministack_udp.h"

/* Import table — slot numbers match exports.tab */
#define mstack_IMPORTS_start DECLARE_IMPORT_TABLE(mstack, 1, 0)
#define mstack_IMPORTS_end   END_IMPORT_TABLE

#define I_udp_bind            DECLARE_IMPORT(4, udp_bind)
#define I_udp_packet_init     DECLARE_IMPORT(5, udp_packet_init)
#define I_udp_packet_send_ll  DECLARE_IMPORT(6, udp_packet_send_ll)

#endif
