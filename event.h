//
// Created by Haiwei Zhang on 2021/10/20.
//

#ifndef XHTTPD_EVENT_H
#define XHTTPD_EVENT_H

#include <sys/event.h>
#include <sys/types.h>
#include "xhttp.h"
#include "common.h"

#define EVENT_READ  0x0001
#define EVENT_WRITE 0x0002

#define NEVENT 64

struct event {
    struct xhttp *http;

    int listen_fd;

    int kqfd;
    struct event_wrap *changes;
    int n_changes;
    int change_size;
    struct kevent *events;
    int event_size;
};

struct event_wrap {
    struct kevent *kev;
    int used;
};

void event_init(struct xhttp *http);
void event_dispatch(struct event *ev);
void event_dealloc(struct event *ev);
void event_add(struct event *ev, int fd, int events);
void event_del(struct  event *ev, int fd, int events);

#endif //XHTTPD_EVENT_H
