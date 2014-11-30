#include <linux/input.h>
#include <linux/uinput.h>

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/epoll.h>

#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int uifd = 0;
int mfd = 0;
int epfd = 0;

uint8_t btnmask = 0;

struct input_event ie;

#define MOUSEPATH "/dev/input/by-id/usb-Logitech_USB_Receiver-if01-event-mouse"

#define MAX_EVENTS 8

#define WRITE_BUFFER_COUNT 4
int main(int argc, char** argv)
{
    struct epoll_event ev;
    struct epoll_event events[MAX_EVENTS];

    /*
     * Mouse Input
     */
    mfd = open(MOUSEPATH, O_RDONLY);
    if (mfd < 0)
    {
        perror("Open " MOUSEPATH);
        exit(1);
    }

    /*
     * Event output
     */
    uifd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (uifd < 0)
    {
        perror("Open /dev/uinput");
        exit(1);
    }

    if (ioctl(uifd, UI_SET_EVBIT, EV_KEY) < 0)
    {
        perror("ioctl:EV_KEY");
        exit(1);
    }

    if (ioctl(uifd, UI_SET_KEYBIT, BTN_LEFT) < 0)
    {
        perror("ioctl:BTN_LEFT");
        exit(1);
    }

    if (ioctl(uifd, UI_SET_KEYBIT, BTN_RIGHT) < 0)
    {
        perror("ioctl:BTN_RIGHT");
        exit(1);
    }

    // apparently evdev needs these
    ioctl(uifd, UI_SET_EVBIT, EV_REL);
    ioctl(uifd, UI_SET_RELBIT, REL_X);
    ioctl(uifd, UI_SET_RELBIT, REL_Y);
    ioctl(uifd, UI_SET_KEYBIT, BTN_MOUSE);

    struct uinput_user_dev uidev;

    memset(&uidev, 0, sizeof(uidev));

    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "automash");
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor  = 0x1d50;
    uidev.id.product = 0x6015;
    uidev.id.version = 11;

    if (write(uifd, &uidev, sizeof(uidev)) < 0)
    {
        perror("write");
        goto failexit;
    }

    if (ioctl(uifd, UI_DEV_CREATE) < 0)
    {
        perror("ioctl");
        goto failexit;
    }

    /*
     * epoll
     */
    epfd = epoll_create(2);
    if (epfd < 0)
    {
        perror("epoll_create");
        goto failexit;
    }

    ev.events = EPOLLIN;
    ev.data.fd = mfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, mfd, &ev) == -1)
    {
        perror("epoll_ctl");
        goto failexit;
    }

    /*
     * main loop
     */
    int nev;

    struct input_event write_buffer[WRITE_BUFFER_COUNT];
    int write_buffer_index = 0;

    for (;;)
    {
        nev = epoll_wait(epfd, events, MAX_EVENTS, btnmask?15:-1);
        if (nev == -1)
        {
            perror("epoll_wait");
            goto failexit;
        }

//         printf("Got %d events\n", nev);

        for (int n = 0; n < nev; n++)
        {
            if (read(events[n].data.fd, &ie, sizeof(struct input_event)) < 0)
            {
                perror("read");
                goto failexit;
            }

            if (ie.type == EV_KEY)
            {
                if (ie.code == BTN_EXTRA)
                {
                    printf("Button LeftMash %s\n", ie.value?"down":"up");
                    btnmask = (btnmask & ~1) | ie.value?1:0;
                }
                if (ie.code == BTN_SIDE)
                {
                    printf("Button RightMash %s\n", ie.value?"down":"up");
                    btnmask = (btnmask & ~2) | ie.value?2:0;
                }
            }
        }

        if (btnmask & 1)
        {
            // issue left click

            // mouse down
            write_buffer[write_buffer_index].type = EV_KEY;
            write_buffer[write_buffer_index].code = BTN_LEFT;
            gettimeofday(&write_buffer[write_buffer_index].time, NULL);
            write_buffer[write_buffer_index].value = 1;
            write_buffer_index++;

            // mouse release
            write_buffer[write_buffer_index].type = EV_KEY;
            write_buffer[write_buffer_index].code = BTN_LEFT;
            gettimeofday(&write_buffer[write_buffer_index].time, NULL);
            write_buffer[write_buffer_index].value = 0;
            write_buffer_index++;

            printf("L");
        }

        if (btnmask & 2)
        {
            // issue right click

            // mouse down
            write_buffer[write_buffer_index].type = EV_KEY;
            write_buffer[write_buffer_index].code = BTN_RIGHT;
            gettimeofday(&write_buffer[write_buffer_index].time, NULL);
            write_buffer[write_buffer_index].value = 1;
            write_buffer_index++;

            // mouse release
            write_buffer[write_buffer_index].type = EV_KEY;
            write_buffer[write_buffer_index].code = BTN_RIGHT;
            gettimeofday(&write_buffer[write_buffer_index].time, NULL);
            write_buffer[write_buffer_index].value = 0;
            write_buffer_index++;

            printf("R");
        }

        // send buffered events
        if (write_buffer_index > 0)
            if (write(uifd, write_buffer, sizeof(write_buffer[0]) * write_buffer_index) < 0)
                goto failexit;
        write_buffer_index = 0;
    }

    ioctl(uifd, UI_DEV_DESTROY);

    close(epfd);
    close(uifd);
    close(mfd);

    return 0;

failexit:
    // FIXME: this is ugly as sin. I know it's wrong, I did it anyway, I'm  sorry

    if (uifd)
        ioctl(uifd, UI_DEV_DESTROY);

    close(epfd);
    close(uifd);
    close(mfd);

    exit(1);
}
