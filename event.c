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
        ev->changes = mem_realloc(ev->changes, sizeof(struct event_wrap) * ev->change_size);
    }
}

void ensure_events_capacity(struct event *ev) {
    if (ev->event_size < ev->n_changes) {
        while (ev->event_size < ev->n_changes)
            ev->event_size *= 2;
        ev->events = mem_realloc(ev->events, sizeof(struct kevent) * ev->event_size);
    }
}

void event_init(struct xhttp *http){
    int kq;
    struct event *ev = NULL;

    if ((kq=kqueue()) == -1)
        log_error(strerror(errno));

    ev = (struct event*)mem_calloc(1, sizeof(struct event));
    ev->kqfd = kq;

    ev->change_size = ev->event_size = NEVENT;
    ev->n_changes = 0;
    ev->changes = mem_calloc(ev->change_size, sizeof(struct kevent));
    ev->events = mem_calloc(ev->change_size, sizeof(struct kevent));
}

void event_dispatch(struct event *ev){
    struct timespec ts;
    ensure_events_capacity(ev);

    ts.tv_sec = 0;
    ts.tv_nsec = 100 * MICRO_SECOND;
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
        struct event_wrap *wrap = (struct event_wrap*)kev.udata;
        wrap->cb(kev.ident, events);
    }
}

void event_dealloc(struct event *ev){
    mem_free(ev);
}

int get_or_construct_event(struct event *ev, int fd, int filter){
    struct event_wrap wrap;
    struct kevent *kev;
    int first_empty = -1;
    for (int i = 0; i < ev->change_size; ++i) {
        wrap = ev->changes[i];
        if (!wrap.used) {
            if (first_empty < 0) first_empty = i;
            continue;
        }
        kev = wrap.kev;
        if (kev->ident == fd && kev->filter == filter)
            return i;
    }
    // not found, construct a new struct
    kev = mem_calloc(1, sizeof(struct kevent));
    kev->ident = fd;
    kev->filter = filter;

    if (first_empty < 0) {
        ensure_changes_capacity(ev, 1);
        first_empty = ev->n_changes;
    }
    ev->n_changes++;
    ev->changes[first_empty].kev = kev;
    ev->changes[first_empty].used = 1;
    return first_empty;
}

void event_add(struct event *ev, int fd, int events){
    struct kevent chev[2];
    struct kevent *kev;
    int pos, n = 0;
    int op = EV_ADD|EV_ENABLE;
    if (events & EVENT_READ) {
        pos = get_or_construct_event(ev, fd, EVFILT_READ);
        kev = ev->changes[pos].kev;
        EV_SET(kev, fd, EVFILT_READ, op, 0, 0, &ev->changes[pos]);
        chev[n++] = *kev;
    }
    if (events & EVENT_WRITE) {
        pos = get_or_construct_event(ev, fd, EVFILT_WRITE);
        kev = ev->changes[pos].kev;
        EV_SET(kev, fd, EVFILT_WRITE, op, 0, 0, &ev->changes[pos]);
        chev[n++] = *kev;
    }
    kevent(ev->kqfd, chev, n, NULL, 0, NULL);
}

void event_del(struct event *ev, int fd, int events){
    struct kevent chev[2];
    struct kevent *kev;
    int pos, n = 0;
    int op = EV_DELETE;
    if (events & EVENT_READ) {
        pos = get_or_construct_event(ev, fd, EVFILT_READ);
        ev->changes[pos].used = 0;
        mem_free(ev->changes[pos].kev);
        ev->n_changes -= 1;
        EV_SET(&chev[n++], fd, EVFILT_READ, op, 0, 0, NULL);
    }
    if (events & EVENT_WRITE) {
        pos = get_or_construct_event(ev, fd, EVFILT_WRITE);
        ev->changes[pos].used = 0;
        mem_free(ev->changes[pos].kev);
        ev->n_changes -= 1;
        EV_SET(&chev[n++], fd, EVFILT_WRITE, op, 0, 0, NULL);
    }
    kevent(ev->kqfd, chev, n, NULL, 0, NULL);
}
