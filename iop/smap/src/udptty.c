#include <thsemap.h>
#include <string.h>
#include <ioman.h>
#include "ministack.h"


static int tty_sema   = -1;
static char ttyname[] = "tty";


static int dummy()
{
    return -5;
}

static int dummy0()
{
    return 0;
}

static int ttyInit(iop_device_t *driver)
{
    iop_sema_t sema_info;

    sema_info.attr    = 0;
    sema_info.initial = 1; /* Unlocked.  */
    sema_info.max     = 1;
    if ((tty_sema = CreateSema(&sema_info)) < 0)
        return -1;

    return 1;
}

static int ttyOpen(int fd, char *name, int mode)
{
    return 1;
}

static int ttyClose(int fd)
{
    return 1;
}

static int ttyWrite(iop_file_t *file, char *buf, int size)
{
    static udp_packet_t pkt;

    WaitSema(tty_sema);

    udp_packet_init(&pkt, 18194);
    memcpy(pkt.payload, buf, size);
    udp_packet_send(&pkt, size);

    SignalSema(tty_sema);

    return size;
}

iop_device_ops_t tty_functarray = {
    ttyInit,
    dummy0,
    (void *)dummy,
    (void *)ttyOpen,
    (void *)ttyClose,
    (void *)dummy,
    (void *)ttyWrite,
    (void *)dummy,
    (void *)dummy,
    (void *)dummy,
    (void *)dummy,
    (void *)dummy,
    (void *)dummy,
    (void *)dummy,
    (void *)dummy,
    (void *)dummy,
    (void *)dummy};

iop_device_t tty_driver = {
    ttyname,
    3,
    1,
    "TTY via Udp",
    &tty_functarray};

int ttyMount(void)
{
    close(0);
    close(1);
    DelDrv(ttyname);
    AddDrv(&tty_driver);
    if (open("tty00:", O_RDONLY) != 0)
        while (1)
            ;
    if (open("tty00:", O_WRONLY) != 1)
        while (1)
            ;

    return 0;
}
