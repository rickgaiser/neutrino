#ifndef EE_CORE_FLAG_H
#define EE_CORE_FLAG_H

// This file is also used by assembly (.S) code
// So keep it simple

#define EECORE_FLAG_UNHOOK      (1<<0) // Unhook syscalls
#define EECORE_FLAG_GSM_FLD_FP  (1<<1) // GSM: Field Mode: Force Progressive
#define EECORE_FLAG_GSM_FRM_FP1 (1<<2) // GSM: Frame Mode: Force Progressive (240p)
#define EECORE_FLAG_GSM_FRM_FP2 (1<<3) // GSM: Frame Mode: Force Progressive (line-double)
#define EECORE_FLAG_GSM_NO_576P (1<<4) // GSM: Disable GSM 576p mode
#define EECORE_FLAG_GSM_C_1     (1<<5) // GSM: Enable FIELD flip type 1
#define EECORE_FLAG_GSM_C_2     (1<<6) // GSM: Enable FIELD flip type 2
#define EECORE_FLAG_GSM_C_3     (1<<7) // GSM: Enable FIELD flip type 3

#endif
