#include <types.h>
#include <thbase.h>
#include <stdio.h>
#include <sysclib.h>
#include <intrman.h>
#include <loadcore.h>
#include <cdvdman.h>
#include "esrimp.h"
#include "ioplib.h"
#include "../../cdvdman_esr1/src/scecdvdv.h"
#include "mprintf.h"

#define MODNAME "ESR_DVDV_PA"
IRX_ID(MODNAME, 0x01, 0x01);

static void **def_sceCdCallback,
    *hook_sceCdCallback,
    **def_sceCdstm0Cb,
    *hook_sceCdstm0Cb,
    **def_sceCdstm1Cb,
    *hook_sceCdstm1Cb,
    **def_sceCdRead0,
    *hook_sceCdRead0,
    **def_sceCdMmode,
    *hook_sceCdMmode,
    *discType,
    //*func_sceCdGetDiscType,
    **def_RegisterIntrHandler,
    *hook_RegisterIntrHandler,
    **def_AllocSysMemory,
    *hook_AllocSysMemory;

static u8 cdvdRead(u8 num)
{
    M_DEBUG("%s(%d)\n", __FUNCTION__, num);

    return (*(vu8 *)(0xbf402000 + num));
}

/*
 * hook function for BIOS CDVDMAN (export 12)
 */
static int hook_sceCdGetDiskType()
{
    M_DEBUG("%s()\n", __FUNCTION__);

    int ret = cdvdRead(0x0f);
    if (ret == SCECdDVDV)
        ret = SCECdPS2DVD;
    return ret;
}

int (*def_sceCdRead)(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode);

static int readSync(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode)
{
    M_DEBUG("%s(%d, %d, 0x%x, ...)\n", __FUNCTION__, lsn, sectors, buf);

    if (sceCdReadDVDV(lsn, sectors, buf, mode)) {
        sceCdSync(0);
        return sceCdGetError();
    } else {
        return 1;
    }
}

/*
 * hook function for BIOS CDVDMAN (export 6)
 */
static u8 sectorData[2064];
static int hook_sceCdRead(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode)
{
    M_DEBUG("%s(%d, %d, 0x%x, ...)\n", __FUNCTION__, lsn, sectors, buf);

    if (cdvdRead(0x0f) == SCECdDVDV) {
        int i;
        u32 consecutiveSectors = (sectors > 2) ? (sectors * 2048) / 2064 : 0;
        if (consecutiveSectors > 0) {
            if (readSync(lsn, consecutiveSectors, buf, mode))
                return 0;
        }
        for (i = 0; i < consecutiveSectors; i++)
            memmove(buf + (2048 * i), buf + (2064 * i) + 12, 2048);

        for (i = consecutiveSectors; i < sectors; i++) {
            if (readSync(lsn + i, 1, sectorData, mode))
                return 0;
            memmove(buf + (2048 * i), sectorData + 12, 2048);
        }
        return 1;
    } else {
        return def_sceCdRead(lsn, sectors, buf, mode);
    }
}

static inline int is_jmp(void *addr, void *func)
{
    if (addr != NULL && *(u32 *)addr == (0x0C000000 | (((u32)func >> 2) & 0x03FFFFFF)))
        return 1;
    return 0;
}

static inline void set_jmp(void *addr, void *func)
{
    *(u32 *)addr = (0x0C000000 | (((u32)func >> 2) & 0x03FFFFFF));
}

int _start(int argc, char **argv)
{
    int rv, oldstate;
    u32 *memAddr;
    u8 currentDiscType;
    iop_library_t *lib;

    M_DEBUG("%s()\n", __FUNCTION__);

    CpuSuspendIntr(&oldstate);

    lib = ioplib_getByName("cdvdman\0");
    // if (lib->version == 0x101 || lib->version == 0x201) {
    if (ioplib_getTableSize(lib) == 62) { // FIXME: is this detection correct ?!
        /*
         * Detected BIOS version of CDVDMAN
         * Do simple hooks and stay resident
         */
        def_sceCdRead = ioplib_hookExportEntry(lib, 6, hook_sceCdRead);
        ioplib_hookExportEntry(lib, 12, hook_sceCdGetDiskType);
        ioplib_relinkExports(lib);

        for (memAddr = 0x0; memAddr < (u32 *)0x1ffffc; memAddr++) {
            if (is_jmp(memAddr, def_sceCdRead))
                set_jmp(memAddr, hook_sceCdRead);
        }
        rv = MODULE_RESIDENT_END;
    } else {
        /*
         * Detected non-BIOS version of CDVDMAN (game)
         * Hook up the other module
         */
        def_sceCdCallback = in_out_func(enum_def_sceCdCallback);
        hook_sceCdCallback = in_out_func(enum_hook_sceCdCallback);
        def_sceCdstm0Cb = in_out_func(enum_def_sceCdstm0Cb);
        hook_sceCdstm0Cb = in_out_func(enum_hook_sceCdstm0Cb);
        def_sceCdstm1Cb = in_out_func(enum_def_sceCdstm1Cb);
        hook_sceCdstm1Cb = in_out_func(enum_hook_sceCdstm1Cb);
        def_sceCdRead0 = in_out_func(enum_def_sceCdRead0);
        hook_sceCdRead0 = in_out_func(enum_hook_sceCdRead0);
        def_sceCdMmode = in_out_func(enum_def_sceCdMmode);
        hook_sceCdMmode = in_out_func(enum_hook_sceCdMmode);
        discType = in_out_func(enum_discType);
        def_RegisterIntrHandler = in_out_func(enum_def_RegisterIntrHandler);
        hook_RegisterIntrHandler = in_out_func(enum_hook_RegisterIntrHandler);
        def_AllocSysMemory = in_out_func(enum_def_AllocSysMemory);
        hook_AllocSysMemory = in_out_func(enum_hook_AllocSysMemory);

        *def_sceCdCallback = ioplib_hookExportEntry(lib, 37, hook_sceCdCallback);
        *def_sceCdstm0Cb = ioplib_hookExportEntry(lib, 48, hook_sceCdstm0Cb);
        *def_sceCdstm1Cb = ioplib_hookExportEntry(lib, 49, hook_sceCdstm1Cb);
        *def_sceCdRead0 = ioplib_hookExportEntry(lib, 62, hook_sceCdRead0);
        *def_sceCdMmode = ioplib_hookExportEntry(lib, 75, hook_sceCdMmode);
        ioplib_relinkExports(lib);

        lib = ioplib_getByName("intrman\0");
        *def_RegisterIntrHandler = ioplib_hookExportEntry(lib, 4, hook_RegisterIntrHandler);
        ioplib_relinkExports(lib);

        lib = ioplib_getByName("sysmem\0\0");
        *def_AllocSysMemory = ioplib_hookExportEntry(lib, 4, hook_AllocSysMemory);
        ioplib_relinkExports(lib);

        // Scanning entire IOP memory for direct uses of functions and values
        //(not using cdvdman's and cdvdfsv's exports). Make sure we're not
        // patching ourselves. This should be rewritten to something more extendable
        for (memAddr = 0x0; memAddr < (u32 *)0x1ffffc; memAddr++) {
            if (is_jmp(memAddr, *def_sceCdRead0))
                set_jmp(memAddr, hook_sceCdRead0);
            else if (is_jmp(memAddr, *def_sceCdCallback))
                set_jmp(memAddr, hook_sceCdCallback);
            else if (is_jmp(memAddr, *def_sceCdstm1Cb))
                set_jmp(memAddr, hook_sceCdstm1Cb);
            else if (is_jmp(memAddr, *def_sceCdstm0Cb))
                set_jmp(memAddr, hook_sceCdstm0Cb);
            // all my reads use "lui v0/v1, 0xbf40" and "lbu v0/v1, 0x200f(v0/v1)"
            // instead of lui+ori+lbu combo, but always make sure
            else if (*memAddr == 0x3c02bf40 && *(memAddr + 1) == 0x3442200f) // lui v0, 0xBF40; ori v0, 0x200F;
            {
                M_DEBUG("Found v0 at: 0x%08x\n", (u32)memAddr);
                *memAddr = 0x3c020000 | ((((u32)discType) >> 16) & 0xFFFF);
                *(memAddr + 1) = 0x34420000 | (((u32)discType) & 0xFFFF);
            } else if (*memAddr == 0x3c03bf40 && *(memAddr + 1) == 0x3463200f) // lui v1, 0xBF40; ori v1, 0x200F;
            {
                M_DEBUG("Found v1 at: 0x%08x\n", (u32)memAddr);
                *memAddr = 0x3c030000 | ((((u32)discType) >> 16) & 0xFFFF);
                *(memAddr + 1) = 0x34630000 | (((u32)discType) & 0xFFFF);
            }
            // beqz reg, label
            // lui v0, 0xBF40
            //...code...
            // lui v0, 0xBF40?
            // label:
            // ori v0, 0x200F
            else if ((*memAddr & 0x10000000) && *(memAddr + 1) == 0x3c02bf40 && (*(memAddr + *(s16 *)memAddr + 1) == 0x3442200f)) { // it never happens with branch going back
                M_DEBUG("Found beqz v0 at: 0x%08x\n", (u32)memAddr);
                *(memAddr + 1) = 0x3c020000 | (((u32)discType >> 16) & 0xFFFF);
                *(memAddr + *(s16 *)memAddr + 1) = 0x34420000 | (((u32)discType) & 0xFFFF);
                if (*(memAddr + *(s16 *)memAddr) == 0x3c02bf40)
                    *(memAddr + *(s16 *)memAddr) = *(memAddr + 1);
            }
        }
        rv = MODULE_NO_RESIDENT_END;
    }
    CpuResumeIntr(oldstate);
    FlushDcache();
    FlushIcache();

    sceCdInit(1);

    do {
        currentDiscType = cdvdRead(0x0f);
    } while (currentDiscType == 0xFF || currentDiscType <= 0x05);

    return rv;
}
