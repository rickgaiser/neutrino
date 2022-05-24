#ifndef IOPRP_H
#define IOPRP_H


struct romdir_entry
{
    char fileName[10];
    unsigned short int extinfo_size;
    unsigned int fileSize;
};


void print_romdir(struct romdir_entry *romdir);


#endif
