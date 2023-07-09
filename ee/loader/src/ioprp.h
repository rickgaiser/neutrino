#ifndef IOPRP_H
#define IOPRP_H


#include <stdint.h>


typedef struct romdir_entry
{
    char name[10];
    uint16_t extinfo_size;
    uint32_t size;
} romdir_entry_t;

typedef struct extinfo {
	uint16_t value;	/* Only applicable for the version field type. */
	uint8_t ext_length;	/* The length of data appended to the end of this entry. */
	uint8_t type;
} extinfo_t;

enum EXTINFO_TYPE {
	EXTINFO_TYPE_DATE=1,
	EXTINFO_TYPE_VERSION,
	EXTINFO_TYPE_COMMENT,
	EXTINFO_TYPE_NULL=0x7F
};


void print_extinfo(const uint8_t *data, uint32_t size);
void print_romdir(const struct romdir_entry *romdir);


#endif
