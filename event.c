//
// Created by Haiwei Zhang on 2021/10/20.
//

#include "event.h"
#include "error.h"
#include "memory.h"

void ensure_changes_capacity(struct event *ev, int n_need) {
    if (ev->n_changes + n_need > ev->change_size) {
        while (ev->n_changes + n_need > ev->change_size)
            ev->change_size *= 2;
        ev->changes = mem_realloc(ev->changes, sizeof(struct event_change) * ev->change_size);
    }
}

void ensure_events_capacity(struct event *ev) {
    if (ev->event_size < ev->n_changes) {
        while (ev->event_size < ev->n_changes)
            ev->event_size *= 2;
        ev->events = mem_realloc(ev->events, sizeof(struct kevent) * ev->event_size);

        log_debugf(NULL, "enlarge event size to %d", ev->event_size);
    }
}

struct event* event_init(){
    int kq;
    struct event *ev = NULL;

    if ((kq=kqueue()) == -1)
        log_error(strerror(errno));

    ev = (struct event*)mem_calloc(1, sizeof(struct event));
    ev->kqfd = kq;

    ev->address_len = sizeof(struct sockaddr_in);
    ev->address = mem_calloc(1, ev->address_len);

    ev->change_size = ev->event_size = N_EVENT;
    ev->n_changes = 0;
    ev->changes = mem_calloc(ev->change_size, sizeof(struct event_change));
    ev->events = mem_calloc(ev->event_size, sizeof(struct kevent));

    ev->conn_size = N_CONN;
    ev->connections = mem_calloc(ev->conn_size, sizeof(struct conn));

    return ev;
}

void event_dispatch(struct event *ev){
    struct timespec ts;
    ensure_events_capacity(ev);

    ts.tv_sec = 0;
    ts.tv_nsec = 100 * MILLISECOND;
    int n = kevent(ev->kqfd, NULL, 0,
                   ev->events, ev->event_size, &ts);
    if (n == -1){
        if (errno != EINTR)
            log_error(strerror(errno));
        //else system call interrupted by signal, ignore
        return;
    }
    if (n == 0) // timeout
        return;
    for (int i = 0; i < n; ++i) {
        int events = 0;
        struct kevent kev = ev->events[i];
        if (kev.flags & EV_ERROR){
            log_warn(strerror(kev.data));
            continue;
        }
        if (kev.filter == EVFILT_READ)
            events |= EVENT_READ;
        else if (kev.filter == EVFILT_WRITE)
            events |= EVENT_WRITE;
        if (!events)
            continue;
        struct event_change *change = (struct event_change*)kev.udata;
        change->cb(ev, &kev, events);
    }
}

void event_dealloc(struct event *ev){
    mem_free(ev);
}

static int get_or_create_event_change(struct event *ev, int fd, int filter){
    struct event_change change;
    int first_empty = -1;
    for (int i = 0; i < ev->change_size; ++i) {
        change = ev->changes[i];
        if (change.fd < 0) {
            if (first_empty < 0) first_empty = i;
            continue;
        }
        if (change.fd == fd && change.filter == filter)
            return i;
    }
    // not found, construct a new struct
    if (first_empty < 0) {
        ensure_changes_capacity(ev, 1);
        first_empty = ev->n_changes;
    }
    ev->n_changes++;
    ev->changes[first_empty].fd = fd;
    ev->changes[first_empty].filter = filter;
    return first_empty;
}

static int find_event_change(struct event *ev, int fd, int filter) {
    for (int i = 0; i < ev->change_size; ++i) {
        if (ev->changes[i].fd < 0) {
            continue;
        }
        if (ev->changes[i].fd == fd && ev->changes[i].filter == filter)
            return i;
    }
    return -1;
}

void event_add(struct event *ev, int fd, int events, event_cb cb){
    struct kevent chev[2];
    struct kevent *kev;
    int pos, n = 0;
    int op = EV_ADD|EV_ENABLE;
    if (events & EVENT_READ) {
        pos = get_or_create_event_change(ev, fd, EVFILT_READ);
        ev->changes[pos].cb = cb;
        EV_SET(&chev[n++], fd, EVFILT_READ, op, 0, 0, &ev->changes[pos]);
    }
    if (events & EVENT_WRITE) {
        pos = get_or_create_event_change(ev, fd, EVFILT_WRITE);
        ev->changes[pos].cb = cb;
        EV_SET(&chev[n++], fd, EVFILT_WRITE, op, 0, 0, &ev->changes[pos]);
    }
    kevent(ev->kqfd, chev, n, NULL, 0, NULL);
}

void event_del(struct event *ev, int fd, int events){
    struct kevent chev[2];
    struct kevent *kev;
    int pos, n = 0;
    int op = EV_DELETE;
    if (events & EVENT_READ) {
        pos = find_event_change(ev, fd, EVFILT_READ);
        if (pos >= 0) {
            ev->changes[pos].fd = -1;
            ev->n_changes -= 1;
            EV_SET(&chev[n++], fd, EVFILT_READ, op, 0, 0, NULL);
        }
    }
    if (events & EVENT_WRITE) {
        pos = find_event_change(ev, fd, EVFILT_WRITE);
        if (pos > 0) {
            ev->changes[pos].fd = -1;
            ev->n_changes -= 1;
            EV_SET(&chev[n++], fd, EVFILT_WRITE, op, 0, 0, NULL);
        }
    }
    kevent(ev->kqfd, chev, n, NULL, 0, NULL);
}

void set_connect_cb(struct event *ev, void (*cb) (struct conn *c)) {
    ev->on_connection = cb;
}

struct conn* event_get_free_connection(struct event *ev){
    int pos = -1;
    for (int i = 0; i < ev->conn_size; ++i) {
        if (ev->connections[i].pos != i)
            pos = i;
    }
    if (pos < 0) {
        pos = ev->conn_size;
        ev->conn_size *= 2;
        ev->connections = mem_realloc(ev->connections, ev->conn_size * sizeof(struct conn));
    }
    ev->connections[pos].ev = ev;
    ev->connections[pos].pos = pos;

    return &ev->connections[pos];
}

struct conn* event_find_connection(struct event *ev, int fd){
    for (int i = 0; i < ev->conn_size; ++i) {
        if (ev->connections[i].pos != i)
            continue;
        if (ev->connections[i].fd == fd)
            return &ev->connections[i];
    }
    return NULL;
}