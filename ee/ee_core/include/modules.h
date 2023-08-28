#ifndef MODULES_H
#define MODULES_H

typedef struct
{
    const void *ptr;
    unsigned int size;

    unsigned int arg_len;
    const char * args;
} irxptr_t;

typedef struct
{
    irxptr_t *modules;
    int count;
} irxtab_t;

#endif
