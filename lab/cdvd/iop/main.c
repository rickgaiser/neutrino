#include <cdvdman.h>
#include <ioman.h>
#include <stdio.h>
#include <sysmem.h>
#include <thbase.h>
#include <thsemap.h>

#define MODNAME "cdvdtest"
IRX_ID(MODNAME, 1, 1);

// Read buffer
#define BUF_SECTORS (128)
#define BUF_SIZE    (BUF_SECTORS*2048)
static unsigned char buffer[BUF_SIZE] __attribute__((aligned(16)));
void test_read(const char *filename)
{
    int ret;
    sceCdlFILE fp = {
        .lsn = 10
    };
    sceCdRMode mode = {
        .trycount = 0,
        .spindlctrl = SCECdSpinNom,
        .datapattern = SCECdSecS2048,
        .pad = 0
    };

    ret = sceCdInit(SCECdINIT);
    //if (ret == 0) {
        printf("%s: sceCdInit = %d, err = %d\n", __FUNCTION__, ret, sceCdGetError());
    //    return;
    //}

    int dt = sceCdGetDiskType();
    printf("%s: sceCdGetDiskType() = %d\n", __FUNCTION__, dt);
    printf("%s: sceCdStatus() = %d\n", __FUNCTION__, sceCdStatus());

    ret = sceCdMmode(dt);
    //if (ret == 0) {
        printf("%s: sceCdMmode(dt) = %d, err = %d\n", __FUNCTION__, ret, sceCdGetError());
    //    return;
    //}

    ret = sceCdSearchFile(&fp, filename);
    //if (ret == 0) {
        printf("%s: sceCdSearchFile(..., %s) = %d, err = %d\n", __FUNCTION__, filename, ret, sceCdGetError());
    //    return;
    //}

    ret = sceCdRead(fp.lsn, BUF_SECTORS, buffer, &mode);
    if (ret == 0) {
        printf("%s: sceCdRead(%d, %d, 0x%x, ...) = %d, err = %d\n", __FUNCTION__, fp.lsn, BUF_SECTORS, buffer, ret, sceCdGetError());
        return;
    }
#if 1
    // Blocking
    ret = sceCdSync(0);
    if (ret != 0) {
        printf("%s: sceCdSync(0) = %d, err = %d\n", __FUNCTION__, ret, sceCdGetError());
        return;
    }
#else
    // NON-Blocking
    while(sceCdSync(1)) {
        nanosleep((const struct timespec[]){{1, 0}}, NULL);
    }
#endif
}

//--------------------------------------------------------------
int _start()
{
    printf("IOP side IOPRP tester\n");

    test_read("\\afs00.afs;1");

    return 1;
}
