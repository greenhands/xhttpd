//
// Created by Haiwei Zhang on 2021/10/20.
//

#ifndef XHTTPD_EVENT_H
#define XHTTPD_EVENT_H

#include "xhttp.h"
#include "common.h"
#include "connection.h"

#define EVENT_READ  0x0001
#define EVENT_WRITE 0x0002

#define N_EVENT         64
#define N_CONN          64
#define N_FD_CLOSE      1024

#define MILLISECOND 1000000

typedef void (*event_cb) (struct event *ev, struct kevent *kev, int events);

struct event {
    struct xhttp *http;

    int kqfd;
    struct event_change *changes;
    int n_changes;
    int change_size;
    struct kevent *events;
    int event_size;

    int fd_close[N_FD_CLOSE];
    int fd_close_len;

    struct conn *connections;
    int conn_size;

    struct sockaddr_in *address; //used for receive connection
    socklen_t address_len;

    void (*on_connection) (struct conn *c);
};

struct event_change {
    int fd;
    int filter;

    event_cb cb;
};

struct event* event_init();
void event_dispatch(struct event *ev);
void event_add(struct event *ev, int fd, int events, event_cb cb);
void event_del(struct  event *ev, int fd, int events);
void event_dealloc(struct event *ev);
int event_close_fd(struct event *ev, int fd);

void set_connect_cb(struct event *ev, void (*cb) (struct conn *c));

struct conn* event_get_free_connection(struct event *ev);
struct conn* event_find_connection(struct event *ev, int fd);

#endif //XHTTPD_EVENT_H
