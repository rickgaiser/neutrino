#include <thsemap.h>
#include <string.h>
#include <ioman.h>
#include "ministack.h"


static int tty_sema   = -1;
static char ttyname[] = "tty";
static udp_packet_t pkt;


static int dummy_m5() { return -5; }
static int dummy_0()  { return 0; }
static int dummy_1()  { return 1; }

static int ttyInit(iop_device_t *driver)
{
    iop_sema_t sema_info;
    sema_info.attr    = 0;
    sema_info.initial = 1; /* Unlocked.  */
    sema_info.max     = 1;
    if ((tty_sema = CreateSema(&sema_info)) < 0)
        return -1;

    // Broadcast packet to UDPTTY port
    udp_packet_init(&pkt, IP_ADDR(255,255,255,255), 18194);

    return 1;
}

static int ttyWrite(iop_file_t *file, void *buf, int size)
{
    // Dummy socket, we do not want anyone to answer
    udp_socket_t socket = {0,NULL,NULL};

    WaitSema(tty_sema);
    memcpy(pkt.payload, buf, size);
    udp_packet_send(&socket, &pkt, size);
    SignalSema(tty_sema);

    return size;
}

iop_device_ops_t tty_functarray = {
    ttyInit,
    (void *)dummy_0,
    (void *)dummy_m5,
    (void *)dummy_1, // open
    (void *)dummy_1, // close
    (void *)dummy_m5,
    ttyWrite,
    (void *)dummy_m5,
    (void *)dummy_m5,
    (void *)dummy_m5,
    (void *)dummy_m5,
    (void *)dummy_m5,
    (void *)dummy_m5,
    (void *)dummy_m5,
    (void *)dummy_m5,
    (void *)dummy_m5,
    (void *)dummy_m5};

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
