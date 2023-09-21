#ifndef ELF_H
#define ELF_H


#include <stdint.h>


#define ELF_MAGIC   0x464c457f // ".ELF"
#define ELF_PT_LOAD 1


typedef struct
{
    uint8_t ident[16]; // struct definition for ELF object header
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} elf_header_t;

typedef struct
{
    uint32_t type; // struct definition for ELF program section header
    uint32_t offset;
    void *vaddr;
    uint32_t paddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
} elf_pheader_t;


#endif
