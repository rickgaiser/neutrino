#ifndef COMPAT_H
#define COMPAT_H


#include <stdint.h>


void get_compat_flag(uint32_t flags, uint32_t *eecore, uint32_t *cdvdman, const char **ioppatch);
void get_compat_game(const char *id, uint32_t *eecore, uint32_t *cdvdman, const char **ioppatch);
void *get_modstorage(const char *id);


#endif
