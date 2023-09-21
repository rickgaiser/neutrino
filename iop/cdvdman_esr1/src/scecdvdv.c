#include <stdio.h>
#include <thbase.h>
#include <loadcore.h>
#include <cdvdman.h>
#include <sysmem.h>
#include "scecdvdv.h"
#include "libcdvd-common.h"
#include "mprintf.h"

extern struct irx_export_table _exp_esr_sl;

#define MODNAME "ESR_DVDV_SL"
IRX_ID(MODNAME, 0x01, 0x01);

// changed from function to macro (volatile not needed)
#define readDiscType(...) (*(u8 *)(0xbf40200f))

// busy but smaller
void waitSync()
{
    M_DEBUG("%s()\n", __FUNCTION__);

    while ((*(u8 *)(0xbf402005) & 0xC0) != 0x40)
        ;
}

#define SECTOR_SIZE (2064)
enum enReadPhase {
    rpNone = 0,
    rpStarted,
    rpContinue
};

// Keeping these in one struct lets the compiler create much smaller code
// by using a single register in function to address them all. Doesn't
// look nice, but even 200 bytes are welcome.
struct tAllValues
{
    u32 startSector;
    u32 totalSectors;
    u32 consecutiveSectors;
    u8 *sceBuffer;
    u8 *readBuff;
    void (*origReadCallback)(int reason);
    int (*def_sceCdCallback)(void (*p)(int reason));
    int (*def_registerIntrHandler)(int irq, int mode, int (*handler)(void *), void *arg);
    void *(*def_allocSysMemory)(u32 mode, u32 size, void *ptr);
    int (*def_sceCdRead0)(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode);
    void (*streamCallback1)(int reason);
    void (*streamCallback0)(int reason);
    int (*def_sceCdstm1Cb)(void (*p)(int reason));
    int (*def_sceCdstm0Cb)(void (*p)(int reason));
    int (*def_sceCdMmode)(int mode);

    s32 *cdvdIntrData;
    u8 readPhase;
    u8 readBuffSize;
    u8 discType;
};

static struct tAllValues allValues; // static, zero initialized

int hookRegisterIntrHandler(int irq, int mode, int (*handler)(void *), void *arg)
{
    M_DEBUG("%s(%d, %d, 0x%x, 0x%x)\n", __FUNCTION__, irq, mode, handler, arg);

    int ret = allValues.def_registerIntrHandler(irq, mode, handler, arg);
    if (irq == 2 && mode == 1) {
        M_DEBUG("Registered interrupt: %d, %d, %08lx, %08lx\n", irq, mode, (u32)handler, (u32)arg);
        allValues.cdvdIntrData = (s32 *)arg;
        allValues.def_sceCdCallback(readCallback);
        allValues.origReadCallback = NULL;
    }
    return ret;
}

// I need all the space I can get. The buffer is always aligned and the size is multiples of 4.
void my_memcpy(void *dest, const void *src, size_t size)
{
    asm volatile(
        ".set noreorder\r\n"
        "loop:\r\n"
        "lw $v0, 0($a1)\n\t"
        "addiu $a1, 4\r\n"
        "sw $v0, 0($a0)\n\t"
        "addi $a2, -4\n\t"
        "bgtz $a2, loop\n\t"
        "addiu $a0, 4\r\n"
        ".set reorder\r\n");
}

void *hookAllocSysMemory(u32 mode, u32 size, void *ptr)
{
    u32 freeMem = QueryMaxFreeMemSize();

    M_DEBUG("%s(%d, %d, 0x%x)\n", __FUNCTION__, mode, size, ptr);
    M_DEBUG("Free memory: %lu, size: %lu\n", freeMem, size);

    if (freeMem < size && allValues.readBuffSize > 1) {
        // there should be a proper synchronization and interrupts should be disabled,
        // but we can't afford that
        while (*(vu8 *)&allValues.readPhase != 0)
            ;
        FreeSysMemory(allValues.readBuff);
        allValues.readBuff = NULL;
    }
    return allValues.def_allocSysMemory(mode, size, ptr);
}

void readRemaining()
{
    u32 i;
    int result;

    M_DEBUG("%s()\n", __FUNCTION__);

    if (allValues.consecutiveSectors > 0) {
        for (i = 0; i < allValues.consecutiveSectors; i++) {
            my_memcpy(allValues.sceBuffer + (2048 * i), allValues.sceBuffer + (2064 * i) + 12, 2048);
        }
        allValues.readPhase = rpContinue;
        for (i = allValues.consecutiveSectors; i < allValues.totalSectors; i++) {
            cd_read_mode_t sceMode = {0, 0, 0, 0};
            result = sceCdRV(allValues.startSector + i, 1, allValues.readBuff, &sceMode, 0, 0);
            if (result != 0) {
                waitSync();
                if (sceCdGetError() != 0)
                    goto onError;
            } else {
                goto onError;
            }
            my_memcpy(allValues.sceBuffer + (2048 * i), allValues.readBuff + 12, 2048);
        }
    } else {
        for (i = 0; i < allValues.totalSectors; i++)
            my_memcpy(allValues.sceBuffer + (2048 * i), allValues.readBuff + (2064 * i) + 12, 2048);
    }
    allValues.readPhase = rpNone;
    return;
onError:
    M_DEBUG("Error readRemaining\n");
    allValues.readPhase = rpNone;
    return;
}

void readCallback(int reason)
{
    M_DEBUG("%s(%d) phase = %d, readType = %ld\n", __FUNCTION__, reason, allValues.readPhase, allValues.cdvdIntrData[3]);

    if (reason == SCECdFuncRead && allValues.readPhase == rpStarted && allValues.cdvdIntrData[3] <= 0) {
        readRemaining();
        if (allValues.origReadCallback)
            allValues.origReadCallback(reason);
    } else if (allValues.readPhase == rpNone && allValues.origReadCallback) {
        allValues.origReadCallback(reason);
    }
}

int hooksceCdRead0(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode)
{
    s32 readType = allValues.cdvdIntrData[3];

    M_DEBUG("%s(%d, %d, 0x%x, ...)\n", __FUNCTION__, lsn, sectors, buf);

    if (readDiscType() == SCECdDVDV /* || readDiscType() == SCECdPS2DVD*/) {
        allValues.consecutiveSectors = (sectors * 2048) / 2064;
        allValues.startSector = lsn;
        allValues.totalSectors = sectors;
        allValues.sceBuffer = (u8 *)buf;

        allValues.readPhase = rpStarted;

        if (!allValues.readBuff || (readType <= 0 && allValues.readBuffSize > 16) || (readType > 0 && allValues.readBuffSize < sectors)) {
            if (allValues.readBuff)
                FreeSysMemory(allValues.readBuff);

            if (readType > 0)
                allValues.readBuffSize = sectors;
            else
                allValues.readBuffSize = 16; // sectors might work better and add single "allValues.readBuffSize < sectors" above?

            allValues.readBuff = allValues.def_allocSysMemory(ALLOC_LAST, SECTOR_SIZE * allValues.readBuffSize, NULL);

            if (!allValues.readBuff) {
                allValues.readBuffSize = 1;
                allValues.readBuff = allValues.def_allocSysMemory(ALLOC_LAST, SECTOR_SIZE, NULL);
                if (!allValues.readBuff) {
                    M_DEBUG("Allocate failed completely\n");
                    while (1)
                        DelayThread(100000); // we can't do anything at this point
                }
                M_DEBUG("Allocate failed: %u\n", allValues.readBuffSize);
            } else {
                M_DEBUG("Allocated: %u\n", allValues.readBuffSize);
            }
        }

        if (allValues.readBuffSize >= sectors)
            allValues.consecutiveSectors = 0;

        if (allValues.consecutiveSectors > 0) {
            if (sceCdRV(lsn, allValues.consecutiveSectors, buf, mode, 0, 0) == 0)
                goto onError;
        } else {
            if (sceCdRV(lsn, sectors, allValues.readBuff, mode, 0, 0) == 0)
                goto onError;
        }
        return 1;
    } else {
        return allValues.def_sceCdRead0(lsn, sectors, buf, mode);
    }
onError:
    M_DEBUG("Error\n");
    allValues.readPhase = rpNone;
    return 0;
}

void streamCallbackMulti(int reason, void (*callback)(int reason))
{
    if (allValues.readPhase == rpStarted) {
        readRemaining();
        if (callback)
            callback(reason);
    } else if (allValues.readPhase == rpNone && callback) {
        callback(reason);
    }
}

void hook_cdvdman_cdstm1cb(int reason)
{
    M_DEBUG("cdstm1CB reason = %d, phase = %d, readType = %ld\n", reason, allValues.readPhase, allValues.cdvdIntrData[3]);
    streamCallbackMulti(reason, allValues.streamCallback1);
}

void hook_cdvdman_cdstm0cb(int reason)
{
    M_DEBUG("cdstm0CB reason = %d, phase = %d, readType = %ld\n", reason, allValues.readPhase, allValues.cdvdIntrData[3]);
    streamCallbackMulti(reason, allValues.streamCallback0);
}

int hook_sceCdCallback(void (*p)(int reason))
{
    int ret = (int)allValues.origReadCallback;
    M_DEBUG("_sceCdCallback(0x%08x)\n", (unsigned int)p);
    allValues.origReadCallback = p;
    return ret;
}

int hook_sceCdstm1Cb(void (*p)(int reason))
{
    M_DEBUG("_sceCdstm1Cb(0x%08x)\n", (unsigned int)p);
    allValues.streamCallback1 = p;
    allValues.def_sceCdstm1Cb(hook_cdvdman_cdstm1cb);
    return 0;
}

int hook_sceCdstm0Cb(void (*p)(int reason))
{
    M_DEBUG("_sceCdstm0Cb(0x%08x)\n", (unsigned int)p);
    allValues.streamCallback0 = p;
    allValues.def_sceCdstm0Cb(hook_cdvdman_cdstm0cb);
    return 0;
}

int hook_sceCdMmode(int mode)
{
    M_DEBUG("sceCdMmode, val = %d\n", mode);
    if (mode != CdMmodeDvd) {
        if (readDiscType() == SCECdDVDV || readDiscType() == SCECdPS2DVD)
            mode = CdMmodeDvd;
    }
    return allValues.def_sceCdMmode(mode);
}

// This is needed because some modules read the value directly and I patch it to point to my own variable.
// If it wasn't for this, the thread would be necessary.
void cdTypeThread()
{
    u8 tmpDiscType = 0;
    while (1) {
        tmpDiscType = readDiscType();
        if (tmpDiscType == 0xFF || tmpDiscType <= 0x05) {
            allValues.discType = tmpDiscType;
            DelayThread(100); // very short delays during media change/detection
        } else {
            if (tmpDiscType == SCECdDVDV)
                tmpDiscType = SCECdPS2DVD;
            allValues.discType = tmpDiscType;
            DelayThread(100000);
        }
    }
}

int _start(int argc, char **argv)
{
    M_DEBUG("%s\n", __FUNCTION__);

    static struct _iop_thread param = {
        attr : 0x02000000,
        option : 0,
        thread : (void *)cdTypeThread,
        stacksize : 0x200,
        priority : 40
    };
    int th;

    RegisterLibraryEntries(&_exp_esr_sl);

    th = CreateThread(&param);

    if (th > 0) {
        StartThread(th, 0);
        return MODULE_RESIDENT_END;
    } else
        return MODULE_NO_RESIDENT_END;
}

void *in_out_func(int type)
{
    static void *functions[] = {
        &allValues.def_sceCdCallback,
        hook_sceCdCallback,
        &allValues.def_sceCdstm0Cb,
        hook_sceCdstm0Cb,
        &allValues.def_sceCdstm1Cb,
        hook_sceCdstm1Cb,
        &allValues.def_sceCdRead0,
        hooksceCdRead0,
        &allValues.def_sceCdMmode,
        hook_sceCdMmode,
        &allValues.discType,
        &allValues.def_registerIntrHandler,
        hookRegisterIntrHandler,
        &allValues.def_allocSysMemory,
        hookAllocSysMemory,
    };
    // if (type >= 0 && type <= enum_hook_RegisterIntrHandler)
    return functions[type];
    // return NULL;
}
