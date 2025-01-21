#ifndef EE_CORE_FLAG_H
#define EE_CORE_FLAG_H

// This file is also used by assembly (.S) code
// So keep it simple

#define EECORE_FLAG_UNHOOK      (1<<0) // Unhook syscalls
#define EECORE_FLAG_GSM1        (1<<1) // Enable GSM mode 1
#define EECORE_FLAG_GSM2        (1<<2) // Enable GSM mode 2
#define EECORE_FLAG_GSM_NO_576P (1<<3) // Disable GSM 576p mode

#endif
