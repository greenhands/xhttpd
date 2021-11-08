//
// Created by Haiwei Zhang on 2021/10/20.
//

#include "event.h"
#include "error.h"
#include "memory.h"

void ensure_events_capacity(struct event *ev) {
    if (ev->event_size < ev->n_changes) {
        while (ev->event_size < ev->n_changes)
            ev->event_size *= 2;
        ev->events = mem_realloc(ev->events, sizeof(struct kevent) * ev->event_size);

        log_debugf(NULL, "enlarge event size to: %d", ev->event_size);
    }
}

struct event* event_init(){
    int kq;
    struct event *ev = NULL;

    // TODO: more signals support
    signal(SIGPIPE, SIG_IGN); // ignore SIGPIPE to prevent from crash on sending to a closed socket

    if ((kq=kqueue()) == -1)
        log_error(strerror(errno));

    ev = (struct event*)mem_calloc(1, sizeof(struct event));
    ev->kqfd = kq;

    ev->address_len = sizeof(struct sockaddr_in);
    ev->address = mem_calloc(1, ev->address_len);

    ev->ch_free = mem_calloc(1, sizeof(struct event_change));

    ev->event_size = N_EVENT;
    ev->events = mem_calloc(ev->event_size, sizeof(struct kevent));

    ev->conn_size = N_CONN;
    ev->connections = mem_calloc(ev->conn_size, sizeof(struct conn*));
    struct conn *ptr = mem_calloc(ev->conn_size, sizeof(struct conn));
    for (int i = 0; i < ev->conn_size; ++i) {
        ev->connections[i] = &ptr[i];
    }

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
    // actually close fd, prevent from reuse fd between loop
    for (int i = 0; i < ev->fd_close_len; ++i) {
        if (close(ev->fd_close[i]) < 0) {
            log_warnf(__func__ , "fd: %d error occurs when close: %s", ev->fd_close[i], strerror(errno));
        }
    }
    ev->fd_close_len = 0;
}

void event_dealloc(struct event *ev){
    //TODO: deallocate dynamic memory
}

int event_close_fd(struct event *ev, int fd) {
    if (ev->fd_close_len == N_FD_CLOSE)
        return close(fd);
    else {
        ev->fd_close[ev->fd_close_len++] = fd;
        return 0;
    }
}

static struct event_change* event_change_get_list(struct event *ev, int fd) {
    int key = fd & (N_HASH - 1);
    return &ev->ch_hash[key];
}

static struct event_change* event_change_list_remove(struct event_change *l, struct event_change *ec) {
    struct event_change *p = l;
    while (p->next) {
        if (p->next == ec) {
            p->next = ec->next;
            ec->next = NULL;
            return ec;
        }
        p = p->next;
    }
    return NULL;
}

static void event_change_list_push(struct event_change *l, struct event_change *ec) {
    ec->next = l->next;
    l->next = ec;
}

static struct event_change* event_change_list_new(struct event *ev) {
    struct event_change *free = ev->ch_free;
    if (!free->next) {
        int alloc_size = ev->n_changes > 16 ? ev->n_changes : 16;
        struct event_change *arr = mem_calloc(alloc_size, sizeof(struct event_change));
        free->next = arr;
        for (int i = 0; i < alloc_size-1; ++i) {
            arr[i].next = arr+(i+1);
        }
        log_debugf(__func__ , "alloc new event_change, size: %d", alloc_size);
    }
    return event_change_list_remove(free, free->next);
}

static struct event_change* find_event_change(struct event *ev, int fd, int filter) {
    struct event_change *l = event_change_get_list(ev, fd);
    struct event_change *p = l->next;
    for (; p != NULL; p = p->next) {
        if (p->fd == fd && p->filter == filter)
            return p;
    }
    return NULL;
}

static void add_event_change(struct event *ev, struct event_change *ec) {
    struct event_change *l = event_change_get_list(ev, ec->fd);
    event_change_list_push(l, ec);
    ev->n_changes++;
}

static void remove_event_change(struct event *ev, struct event_change *ec) {
    struct event_change *l = event_change_get_list(ev, ec->fd);
    event_change_list_remove(l, ec);
    event_change_list_push(ev->ch_free, ec);
    ev->n_changes--;
}

static struct event_change* get_or_create_event_change(struct event *ev, int fd, int filter){
    struct event_change *ch = find_event_change(ev, fd, filter);
    if (ch) return ch;
    ch = event_change_list_new(ev);
    ch->fd = fd;
    ch->filter = filter;
    add_event_change(ev, ch);
    return ch;
}

void event_add(struct event *ev, int fd, int events, event_cb cb){
    struct kevent chev[2];
    struct kevent *kev;
    int n = 0;
    struct event_change *ec;
    int op = EV_ADD|EV_ENABLE|EV_ONESHOT;
    if (events & EVENT_READ) {
        ec = get_or_create_event_change(ev, fd, EVFILT_READ);
        ec->cb = cb;
        EV_SET(&chev[n++], fd, EVFILT_READ, op, 0, 0, ec);
    }
    if (events & EVENT_WRITE) {
        ec = get_or_create_event_change(ev, fd, EVFILT_WRITE);
        ec->cb = cb;
        EV_SET(&chev[n++], fd, EVFILT_WRITE, op, 0, 0, ec);
    }
    kevent(ev->kqfd, chev, n, NULL, 0, NULL);
}

void event_del(struct event *ev, int fd, int events){
    struct kevent chev[2];
    struct kevent *kev;
    int n = 0;
    struct event_change *ec;
    int op = EV_DELETE|EV_ONESHOT;
    if (events & EVENT_READ) {
        ec = find_event_change(ev, fd, EVFILT_READ);
        if (ec) {
            remove_event_change(ev, ec);
        }
        EV_SET(&chev[n++], fd, EVFILT_READ, op, 0, 0, NULL);
    }
    if (events & EVENT_WRITE) {
        ec = find_event_change(ev, fd, EVFILT_WRITE);
        if (ec) {
            remove_event_change(ev, ec);
        }
        EV_SET(&chev[n++], fd, EVFILT_WRITE, op, 0, 0, NULL);
    }
    kevent(ev->kqfd, chev, n, NULL, 0, NULL);
}

void set_connect_cb(struct event *ev, void (*cb) (struct conn *c)) {
    ev->on_connection = cb;
}

struct conn* event_get_free_connection(struct event *ev, int fd){
    if (fd >= ev->conn_size) {
        int start = ev->conn_size;
        while (ev->conn_size <= fd)
            ev->conn_size *= 2;
        ev->connections = mem_realloc(ev->connections, ev->conn_size* sizeof(struct conn*));
        struct conn *ptr = mem_calloc(ev->conn_size-start, sizeof(struct conn));
        for (int i = start; i < ev->conn_size; ++i) {
            ev->connections[i] = &ptr[i-start];
        }
        log_debugf(__func__ , "enlarge connection size to: %d", ev->conn_size);
    }
    struct conn *c = ev->connections[fd];
    if (c->fd > 0)
        log_warnf(__func__ , "fd: %d connection reused before free", fd);
    c->fd = fd;
    c->ev = ev;

    return c;
}

struct conn* event_find_connection(struct event *ev, int fd){
    if (fd >= ev->conn_size || ev->connections[fd]->fd < 0)
        return NULL;
    return ev->connections[fd];
}