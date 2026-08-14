#include <kernel.h>

#include "tlb.h"

static int InitTLB32MB(void);

void InitializeTLB(void)
{
    if (GetMemorySize() == 0x2000000)
        InitTLB32MB();
    else
        _InitTLB();
}

static int SetTLBEntry(unsigned int index, unsigned int pageMask, unsigned int entryHi, unsigned int entryLo0, unsigned int entryLo1)
{
    if (index >= 0x30)
        return -1;

    __asm__ volatile(
        "mtc0 %0, $0\n"
        "mtc0 %1, $5\n"
        "mtc0 %2, $10\n"
        "mtc0 %3, $2\n"
        "mtc0 %4, $3\n"
        "sync.p\n"
        "tlbwi\n"
        "sync.p\n"
        :
        : "r"(index), "r"(pageMask), "r"(entryHi), "r"(entryLo0), "r"(entryLo1));

    return index;
}

struct TLBEntry
{
    unsigned int pageMask;
    unsigned int entryHi;
    unsigned int entryLo0;
    unsigned int entryLo1;
};

struct TLBInfo
{
    unsigned int numKernelEntries;
    unsigned int numDefaultEntries;
    unsigned int numExtendedEntries;
    unsigned int numWiredEntries;
    const struct TLBEntry *kernelEntries;
    const struct TLBEntry *defaultEntries;
    const struct TLBEntry *extendedEntries;
};

#define TLB_NUM_KERNEL_ENTRIES 0x0D
#define TLB_NUM_DEFAULT_ENTRIES 0x12
#define TLB_NUM_EXTENDED_ENTRIES 0x08

#if (TLB_NUM_KERNEL_ENTRIES + TLB_NUM_DEFAULT_ENTRIES + TLB_NUM_EXTENDED_ENTRIES >= 0x31)
#error TLB overflow
#endif

static const struct TLBEntry kernelTLB[TLB_NUM_KERNEL_ENTRIES] = {
    {0x00000000, 0x70000000, 0x80000007, 0x00000007},
    {0x00006000, 0xFFFF8000, 0x00001E1F, 0x00001F1F},
    {0x00000000, 0x10000000, 0x00400017, 0x00400053},
    {0x00000000, 0x10002000, 0x00400097, 0x004000D7},
    {0x00000000, 0x10004000, 0x00400117, 0x00400157},
    {0x00000000, 0x10006000, 0x00400197, 0x004001D7},
    {0x00000000, 0x10008000, 0x00400217, 0x00400257},
    {0x00000000, 0x1000A000, 0x00400297, 0x004002D7},
    {0x00000000, 0x1000C000, 0x00400313, 0x00400357},
    {0x00000000, 0x1000E000, 0x00400397, 0x004003D7},
    {0x0001E000, 0x11000000, 0x00440017, 0x00440415},
    {0x0001E000, 0x12000000, 0x00480017, 0x00480415},
    {0x01FFE000, 0x1E000000, 0x00780017, 0x007C0017}};

static const struct TLBEntry defaultTLB[TLB_NUM_DEFAULT_ENTRIES] = {
    {0x0007E000, 0x00080000, 0x0000201F, 0x0000301F},
    {0x0007E000, 0x00100000, 0x0000401F, 0x0000501F},
    {0x0007E000, 0x00180000, 0x0000601F, 0x0000701F},
    {0x001FE000, 0x00200000, 0x0000801F, 0x0000C01F},
    {0x001FE000, 0x00400000, 0x0001001F, 0x0001401F},
    {0x001FE000, 0x00600000, 0x0001801F, 0x0001C01F},
    {0x007FE000, 0x00800000, 0x0002001F, 0x0003001F},
    {0x007FE000, 0x01000000, 0x0004001F, 0x0005001F},
    {0x007FE000, 0x01800000, 0x0006001F, 0x0007001F},
    {0x0007E000, 0x20080000, 0x00002017, 0x00003017},
    {0x0007E000, 0x20100000, 0x00004017, 0x00005017},
    {0x0007E000, 0x20180000, 0x00006017, 0x00007017},
    {0x001FE000, 0x20200000, 0x00008017, 0x0000C017},
    {0x001FE000, 0x20400000, 0x00010017, 0x00014017},
    {0x001FE000, 0x20600000, 0x00018017, 0x0001C017},
    {0x007FE000, 0x20800000, 0x00020017, 0x00030017},
    {0x007FE000, 0x21000000, 0x00040017, 0x00050017},
    {0x007FE000, 0x21800000, 0x00060017, 0x00070017}};

static const struct TLBEntry extendedTLB[TLB_NUM_EXTENDED_ENTRIES] = {
    {0x0007E000, 0x30100000, 0x0000403F, 0x0000503F},
    {0x0007E000, 0x30180000, 0x0000603F, 0x0000703F},
    {0x001FE000, 0x30200000, 0x0000803F, 0x0000C03F},
    {0x001FE000, 0x30400000, 0x0001003F, 0x0001403F},
    {0x001FE000, 0x30600000, 0x0001803F, 0x0001C03F},
    {0x007FE000, 0x30800000, 0x0002003F, 0x0003003F},
    {0x007FE000, 0x31000000, 0x0004003F, 0x0005003F},
    {0x007FE000, 0x31800000, 0x0006003F, 0x0007003F}};

static struct TLBInfo tlbInfo = {
    TLB_NUM_KERNEL_ENTRIES,
    TLB_NUM_DEFAULT_ENTRIES,
    TLB_NUM_EXTENDED_ENTRIES,
    0,
    kernelTLB,
    defaultTLB,
    extendedTLB};

static int InitTLB32MB(void)
{
    unsigned int i;
    unsigned int numEntries;
    unsigned int value;
    unsigned int endIndex;
    const struct TLBEntry *entry;

    __asm__ volatile(
        "mtc0 $zero, $6\n"
        "sync.p\n");

    for (i = 0, entry = tlbInfo.kernelEntries; i < tlbInfo.numKernelEntries; i++, entry++)
        SetTLBEntry(i, entry->pageMask, entry->entryHi, entry->entryLo0, entry->entryLo1);

    entry = tlbInfo.defaultEntries;
    endIndex = tlbInfo.numDefaultEntries + i;
    for (; i < endIndex; i++, entry++)
        SetTLBEntry(i, entry->pageMask, entry->entryHi, entry->entryLo0, entry->entryLo1);

    tlbInfo.numWiredEntries = numEntries = i;
    __asm__ volatile(
        "mtc0 %0, $6\n"
        "sync.p\n"
        :
        : "r"(numEntries));

    entry = tlbInfo.extendedEntries;
    endIndex = tlbInfo.numExtendedEntries + i;
    for (; i < endIndex; i++, entry++, numEntries++)
        SetTLBEntry(i, entry->pageMask, entry->entryHi, entry->entryLo0, entry->entryLo1);

    for (value = 0xE0000000 + (numEntries << 13); i < 0x30; i++, value += 0x2000)
        SetTLBEntry(i, 0, value, 0, 0);

    return numEntries;
}
