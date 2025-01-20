#include <loadcore.h>
#include <sysmem.h>
#include <thbase.h>
#include <tamtypes.h>

#include "mprintf.h"
#include "ioplib.h"

#define MODNAME "memcheck"
IRX_ID(MODNAME, 1, 1);

typedef void * (*fp_AllocSysMemory)(int mode, int size, void *ptr);
typedef int    (*fp_FreeSysMemory)(void *ptr);
//typedef u32    (*fp_QueryMemSize)();
//typedef u32    (*fp_QueryMaxFreeMemSize)();
//typedef u32    (*fp_QueryTotalFreeMemSize)();
typedef void * (*fp_QueryBlockTopAddress)(void *address);
typedef int    (*fp_QueryBlockSize)(void *address);
fp_AllocSysMemory        org_AllocSysMemory;
fp_FreeSysMemory         org_FreeSysMemory;
//fp_QueryMemSize          org_QueryMemSize;
//fp_QueryMaxFreeMemSize   org_QueryMaxFreeMemSize;
//fp_QueryTotalFreeMemSize org_QueryTotalFreeMemSize;
fp_QueryBlockTopAddress  org_QueryBlockTopAddress;
fp_QueryBlockSize        org_QueryBlockSize;

typedef int (*fp_CreateThread)(iop_thread_t *thparam);
fp_CreateThread org_CreateThread;

#define SMBF_USED     1<<0
#define SMBF_CORRUPT  1<<1
struct SMemBlock {
    unsigned char *addr;
    int size;
    unsigned char flags;
};

#define CHECKREGION 256
#define MAX_BLOCKS 256
struct SMemBlock blocks[MAX_BLOCKS];

static void check_block(struct SMemBlock *pmb)
{
    int i, c_start, c_end, count;
    unsigned char *mem_check = &pmb->addr[pmb->size];

    c_start = -1;
    c_end = -1;
    count = 0;
    mem_check = &pmb->addr[0];
    for (i = 0; i < CHECKREGION; i++) {
        if (mem_check[i] != 0xAA) {
            M_DEBUG("[%d] 0x%x = %d\n", i, &mem_check[i], mem_check[i]);

            // Beginning of corrupt area
            if (c_start == -1)
                c_start = i;

            // End of corrupt area
            c_end = i;

            count++;
        }
    }

    if (count > 0) {
        M_DEBUG("\n");
        M_DEBUG("- %d byte guard region BEFORE block corrupt!\n", CHECKREGION);
        M_DEBUG("- first byte: %d @ 0x%x\n", c_start, &mem_check[c_start]);
        M_DEBUG("- last  byte: %d @ 0x%x\n", c_end, &mem_check[c_end]);
        M_DEBUG("- count:      %d\n", count);
        M_DEBUG("- allocation: %dB/%dKiB\n", pmb->size, pmb->size/1024);
        M_DEBUG("- address:    0x%x\n", pmb->addr);
        M_DEBUG("- guard:      0x%x\n", mem_check);
        M_DEBUG("\n");
        pmb->flags |= SMBF_CORRUPT;
    }

    c_start = -1;
    c_end = -1;
    count = 0;
    mem_check = &pmb->addr[CHECKREGION + pmb->size];
    for (i = 0; i < CHECKREGION; i++) {
        if (mem_check[i] != 0xAA) {
            M_DEBUG("[%d] 0x%x = %d\n", i, &mem_check[i], mem_check[i]);

            // Beginning of corrupt area
            if (c_start == -1)
                c_start = i;

            // End of corrupt area
            c_end = i;

            count++;
        }
    }

    if (count > 0) {
        M_DEBUG("\n");
        M_DEBUG("- %d byte guard region AFTER block corrupt!\n", CHECKREGION);
        M_DEBUG("- first byte: %d @ 0x%x\n", c_start, &mem_check[c_start]);
        M_DEBUG("- last  byte: %d @ 0x%x\n", c_end, &mem_check[c_end]);
        M_DEBUG("- count:      %d\n", count);
        M_DEBUG("- allocation: %dB/%dKiB\n", pmb->size, pmb->size/1024);
        M_DEBUG("- address:    0x%x\n", pmb->addr);
        M_DEBUG("- guard:      0x%x\n", mem_check);
        M_DEBUG("\n");
        pmb->flags |= SMBF_CORRUPT;
    }
}

static void check()
{
    int i;
    for (i = 0; i < MAX_BLOCKS; i++)
        if (blocks[i].flags == SMBF_USED)
            check_block(&blocks[i]);
}

static void * hooked_AllocSysMemory(int mode, int size, void *ptr)
{
    int i;
    struct SMemBlock *pmb = NULL;
    unsigned char * rv;

    // Check all previous allocations
    check();

    // Make new allocation
    rv = org_AllocSysMemory(mode, CHECKREGION + size + CHECKREGION, ptr);

    // Find a free entry
    for (i = 0; i < MAX_BLOCKS; i++) {
        if (blocks[i].flags == 0) {
            pmb = &blocks[i];
            break;
        }
    }

    if (pmb != NULL && rv != NULL) {
        // Fill entry
        pmb->addr = rv;
        pmb->size = size;
        pmb->flags = SMBF_USED;

        // Pattern fill extra memory
        for (i = 0; i < CHECKREGION; i++) {
            pmb->addr[i] = 0xAA;
            pmb->addr[CHECKREGION + pmb->size + i] = 0xAA;
        }
        rv += CHECKREGION;
    }

    //M_DEBUG("%s(%d, %dB/%dKiB, 0x%x) = 0x%x\n", __FUNCTION__, mode, size, size/1024, ptr, rv);

    return rv;
}

static int hooked_FreeSysMemory(void *ptr)
{
    int i, rv;

    // Check all previous allocations
    check();

    // Clear entry
    for (i = 0; i < MAX_BLOCKS; i++) {
        if (blocks[i].addr == ((unsigned char *)ptr - CHECKREGION)) {
            blocks[i].flags = 0;
            ptr = (unsigned char *)ptr - CHECKREGION;
            break;
        }
    }

    rv = org_FreeSysMemory(ptr);
    //M_DEBUG("%s(0x%x) = %d\n", __FUNCTION__, ptr, rv);
    return rv;
}

#if 0
static u32 hooked_QueryMemSize()
{
    return org_QueryMemSize();
}

static u32 hooked_QueryMaxFreeMemSize()
{
    return org_QueryMaxFreeMemSize();
}

static u32 hooked_QueryTotalFreeMemSize()
{
    return org_QueryTotalFreeMemSize();
}
#endif

static void * hooked_QueryBlockTopAddress(void *address)
{
    int i;

    // Find entry
    for (i = 0; i < MAX_BLOCKS; i++) {
        if (blocks[i].addr == ((unsigned char *)address - CHECKREGION))
            return blocks[i].addr + CHECKREGION + blocks[i].size;
    }

    return org_QueryBlockTopAddress(address);
}

static int hooked_QueryBlockSize(void *address)
{
    int i;

    // Find entry
    for (i = 0; i < MAX_BLOCKS; i++) {
        if (blocks[i].addr == ((unsigned char *)address - CHECKREGION))
            return blocks[i].size;
    }

    return org_QueryBlockSize(address);
}

int hooked_CreateThread(iop_thread_t *thparam)
{
    M_DEBUG("%s({priority=%d})\n", __FUNCTION__, thparam->priority);
    return org_CreateThread(thparam);
}

int _start(int argc, char **argv)
{
    // Hook sysmem functions
    iop_library_t * lib_sysmem = ioplib_getByName("sysmem");
    org_AllocSysMemory        = ioplib_hookExportEntry(lib_sysmem,  4, hooked_AllocSysMemory);
    org_FreeSysMemory         = ioplib_hookExportEntry(lib_sysmem,  5, hooked_FreeSysMemory);
    //org_QueryMemSize          = ioplib_hookExportEntry(lib_sysmem,  6, hooked_QueryMemSize);
    //org_QueryMaxFreeMemSize   = ioplib_hookExportEntry(lib_sysmem,  7, hooked_QueryMaxFreeMemSize);
    //org_QueryTotalFreeMemSize = ioplib_hookExportEntry(lib_sysmem,  8, hooked_QueryTotalFreeMemSize);
    org_QueryBlockTopAddress  = ioplib_hookExportEntry(lib_sysmem,  9, hooked_QueryBlockTopAddress);
    org_QueryBlockSize        = ioplib_hookExportEntry(lib_sysmem, 10, hooked_QueryBlockSize);
    ioplib_relinkExports(lib_sysmem);

    // Hook thbase functions
    iop_library_t * lib_thbase = ioplib_getByName("thbase");
    org_CreateThread = ioplib_hookExportEntry(lib_thbase,  4, hooked_CreateThread);
    ioplib_relinkExports(lib_thbase);

    return MODULE_RESIDENT_END;
}
