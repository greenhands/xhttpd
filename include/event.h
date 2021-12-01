//
// Created by Haiwei Zhang on 2021/10/20.
//

#ifndef XHTTPD_EVENT_H
#define XHTTPD_EVENT_H

#include "xhttp.h"
#include "common.h"
#include "connection.h"
#include "time_heap.h"

#define EVENT_READ  0x0001
#define EVENT_WRITE 0x0002

#define N_EVENT         64
#define N_CONN          64
#define N_FD_CLOSE      1024
#define N_HASH          (2<<8)

#define NANOSECOND  1
#define MICROSECOND 1000
#define MILLISECOND 1000000
#define SECOND      1000000000

time_msec_t curr_time_msec;

typedef void (*event_cb) (struct event *ev, struct kevent *kev, int events);

struct event_change {
    int fd;
    int filter;

    event_cb cb;
    struct event_change *next;
};

struct event {
    struct xhttp *http;
    struct timer_heap *heap;

    int kqfd;
    int n_changes;
    struct event_change ch_hash[N_HASH];
    struct event_change *ch_free;
    int event_size;
    struct kevent *events;

    int fd_close[N_FD_CLOSE];
    int fd_close_len;

    struct conn **connections;
    int conn_size;

    struct sockaddr_in *address; //used for receive connection
    socklen_t address_len;

    void (*on_connection) (struct conn *c);
    void (*on_exit) (struct xhttp *http);
};

struct event* event_init();
int event_dispatch(struct event *ev);
void event_add(struct event *ev, int fd, int events, event_cb cb);
void event_del(struct event *ev, int fd, int events);
struct timer_node* event_add_timer(struct event *ev, time_msec_t msec, timer_callback cb, void *data);
void event_delete_timer(struct event *ev, struct timer_node *timer);
int event_close_fd(struct event *ev, int fd);

void set_connect_cb(struct event *ev, void (*cb) (struct conn *c));
void set_exit_cb(struct event *ev, void (*cb) (struct xhttp *http));

struct conn* event_get_free_connection(struct event *ev, int fd);
struct conn* event_find_connection(struct event *ev, int fd);

#endif //XHTTPD_EVENT_H
