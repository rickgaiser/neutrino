#ifndef ELF_H
#define ELF_H


#include <tamtypes.h>


#define ELF_MAGIC   0x464c457f // ".ELF"
#define ELF_PT_LOAD 1


typedef struct
{
    u8 ident[16]; // struct definition for ELF object header
    u16 type;
    u16 machine;
    u32 version;
    u32 entry;
    u32 phoff;
    u32 shoff;
    u32 flags;
    u16 ehsize;
    u16 phentsize;
    u16 phnum;
    u16 shentsize;
    u16 shnum;
    u16 shstrndx;
} elf_header_t;

typedef struct
{
    u32 type; // struct definition for ELF program section header
    u32 offset;
    void *vaddr;
    u32 paddr;
    u32 filesz;
    u32 memsz;
    u32 flags;
    u32 align;
} elf_pheader_t;


#endif
