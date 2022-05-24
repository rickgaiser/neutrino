#include "ioprp.h"
#include <stdio.h>


void print_romdir(struct romdir_entry *romdir)
{
    while (romdir->fileName[0] != '\0') {
        printf("- %s [size=%d, extsize=%d]\n", romdir->fileName, romdir->fileSize, romdir->extinfo_size);
        romdir++;
    }
}
