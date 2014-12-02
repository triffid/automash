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

int time_increment_and_copy(struct timeval* time, struct timeval* buffer, unsigned int incr)
{
    if ((sizeof(incr) == 32) && (sizeof(time->tv_usec) == 32))
    {
        if ((time->tv_usec & 1UL<<31) && (incr & 1UL<<31))
        {
            // add will overflow and we won't know
            // instead, subtract half maxint from each then add, and increment seconds
            time->tv_usec = (time->tv_usec - (1UL<<31)) + (incr - (1UL<<31));
            time->tv_sec ++;
        }
    }
    else
    {
        // todo: error
        return -1;
    }

    memcpy(buffer, time, sizeof(*time));

    return 0;
}

void queue_event(int type, int code, int value, struct input_event* queue, int* queue_index, struct timeval* time)
{
    queue[*queue_index].type = type;
    queue[*queue_index].code = code;
    queue[*queue_index].value = value;

    time_increment_and_copy(time, &queue[*queue_index].time, 2000000);

    (*queue_index)++;
}

void failexit(const char* message) __attribute__ ((noreturn));
void failexit(const char* message)
{
    if (uifd > 0)
    {
        ioctl(uifd, UI_DEV_DESTROY);
        close(uifd);
    }

    if (mfd > 0)
        close(mfd);

    if (epfd > 0)
        close(epfd);

    perror(message);
    exit(1);
}

#define WRITE_BUFFER_COUNT 64
int main(int argc, char** argv)
{
    struct epoll_event ev;
    struct epoll_event events[MAX_EVENTS];

    /*
     * Mouse Input
     */
    mfd = open(MOUSEPATH, O_RDONLY);
    if (mfd < 0)
        failexit("open " MOUSEPATH);

    /*
     * Event output
     */
    uifd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (uifd < 0)
        failexit("open /dev/uinput");

    // tell uinput that we send mouse clicks
    ioctl(uifd, UI_SET_EVBIT, EV_KEY);
    ioctl(uifd, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(uifd, UI_SET_KEYBIT, BTN_RIGHT);

    // apparently evdev needs these to recognise our device as a mouse
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
        failexit("write /dev/uinput");

    if (ioctl(uifd, UI_DEV_CREATE) < 0)
        failexit("ioctl uinput create");

    /*
     * epoll
     */
    epfd = epoll_create(2);
    if (epfd < 0)
        failexit("epoll_create");

    ev.events = EPOLLIN;
    ev.data.fd = mfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, mfd, &ev) == -1)
        failexit("epoll_ctl_add");

    /*
     * main loop
     */
    int nev;

    struct input_event write_buffer[WRITE_BUFFER_COUNT];
    int write_buffer_index = 0;

    struct timeval time;

    for (;;)
    {
        nev = epoll_wait(epfd, events, MAX_EVENTS, btnmask?100:-1);
        if (nev == -1)
            failexit("epoll_wait");

        for (int n = 0; n < nev; n++)
        {
            if (read(events[n].data.fd, &ie, sizeof(struct input_event)) < 0)
                failexit("read " MOUSEPATH);

            if (ie.type == EV_KEY)
            {
                if (ie.code == BTN_EXTRA)
                {
//                     printf("Button LeftMash %s\n", ie.value?"down":"up");
                    btnmask = (btnmask & ~1) | ie.value?1:0;
                }
                if (ie.code == BTN_SIDE)
                {
//                     printf("Button RightMash %s\n", ie.value?"down":"up");
                    btnmask = (btnmask & ~2) | ie.value?2:0;
                }
            }
        }

        if (btnmask != 0)
        {
            gettimeofday(&time, NULL);

            if (btnmask & 1)
            {
                // issue 8 left clicks

                for (int i = 25; i; i--)
                {
                    // mouse down
                    queue_event(EV_KEY, BTN_LEFT, 1, write_buffer, &write_buffer_index, &time);

                    // mouse release
                    queue_event(EV_KEY, BTN_LEFT, 0, write_buffer, &write_buffer_index, &time);
                }

//                 printf("L");
            }

            if (btnmask & 2)
            {
                // issue 8 right clicks

                for (int i = 25; i; i--)
                {
                    // mouse down
                    queue_event(EV_KEY, BTN_RIGHT, 1, write_buffer, &write_buffer_index, &time);

                    // mouse release
                    queue_event(EV_KEY, BTN_RIGHT, 0, write_buffer, &write_buffer_index, &time);
                }

//                 printf("R");
            }

            // send buffered events
            if (write_buffer_index > 0)
                if (write(uifd, write_buffer, sizeof(write_buffer[0]) * write_buffer_index) < 0)
                    failexit("write uinput event");

            write_buffer_index = 0;
        }
    }

    ioctl(uifd, UI_DEV_DESTROY);

    close(epfd);
    close(uifd);
    close(mfd);

    return 0;
}
