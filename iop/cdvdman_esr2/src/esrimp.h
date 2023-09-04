#ifndef IOP_ESRIMP_H
#define IOP_ESRIMP_H

#include "types.h"
#include "irx.h"

#define esrimp_IMPORTS_start DECLARE_IMPORT_TABLE(esr_sl, 1, 1)
#define esrimp_IMPORTS_end END_IMPORT_TABLE

void *in_out_func(int type);
#define I_in_out_func DECLARE_IMPORT(4, in_out_func)

#endif /* IOP_ESRIMP_H */
