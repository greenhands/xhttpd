//
// Created by Haiwei Zhang on 2021/10/20.
//

#include "event.h"
#include "error.h"
#include "memory.h"

static int signals[] = {SIGTERM, SIGINT, SIGHUP, SIGQUIT, SIGCHLD, -1};
static int pipefd[2];
static int is_quit;

static void ensure_events_capacity(struct event *ev) {
    if (ev->event_size < ev->n_changes) {
        while (ev->event_size < ev->n_changes)
            ev->event_size *= 2;
        ev->events = mem_realloc(ev->events, sizeof(struct kevent) * ev->event_size);

        log_debugf(NULL, "enlarge event size to: %d", ev->event_size);
    }
}

static void handle_signal_event(struct event *ev, struct kevent *kev, int events) {
    int fd = kev->ident;
    log_debugf(__func__ , "signal received, fd: %d", fd);
    char sigs[LINE_SIZE];
    while (1) {
        int n = recv(fd, sigs, LINE_SIZE, 0);
        if (n == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                event_add(ev, fd, EVENT_READ, handle_signal_event);
                return;
            }
            else
                log_errorf(NULL, "read signal pipe err: %s", strerror(errno));
        }
        if (n == 0)
            log_error("signal pipe closed unexpected");
        for (int i = 0; i < n; ++i) {
            switch (sigs[i]) {
                case SIGHUP:
                case SIGCHLD:
                    break;
                default:
                    is_quit = 1;
                    return;
            }
        }
    }
}

static void event_init_pipe(struct event *ev) {
    if (socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd) == -1)
        log_errorf(NULL, "call socketpair() failed: %s", strerror(errno));

    set_nonblocking(pipefd[1]); /* receive signal, should be nonblock */
    event_add(ev, pipefd[1], EVENT_READ, handle_signal_event);
}

static void event_signal_handler(int sig) {
    int save_errno = errno;
    int msg = sig;
    if (send(pipefd[0], (char *)&msg, 1, 0) == -1)
        log_warnf(__func__ , "send signal to pipe filed: %s", strerror(errno));
    errno = save_errno;
}

static void event_init_signals(struct event *ev) {
    signal(SIGPIPE, SIG_IGN); // ignore SIGPIPE to prevent from crash on sending to a closed socket

    struct sigaction sa;
    for (int *sig = signals; *sig > 0 ; sig++) {
        memset(&sa, 0, sizeof(struct sigaction));

        sa.sa_flags |= SA_RESTART;
        sa.sa_handler = event_signal_handler;
        sigemptyset(&sa.sa_mask);

        if (sigaction(*sig, &sa, NULL) == -1)
            log_errorf(NULL, "call sigaction(%d) failed", *sig);
    }
}

// deprecated
static void event_init_periodical_timer(struct event *ev) {
    time_msec_t timer_resolution_msec = 100;

    struct itimerval itv;
    itv.it_interval.tv_sec = timer_resolution_msec / 1000;
    itv.it_interval.tv_usec = (timer_resolution_msec % 1000) * 1000;
    itv.it_value.tv_sec = timer_resolution_msec / 1000;
    itv.it_value.tv_usec = (timer_resolution_msec % 1000) * 1000;
    if (setitimer(ITIMER_REAL, &itv, NULL) == -1)
        log_errorf(NULL, "call setitimer() failed: %s", strerror(errno));
}

void event_update_time(struct event *ev) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == -1)
        log_warnf(__func__ ,"call gettimeofday() failed, %s", strerror(errno));

    curr_time_msec = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    log_debugf(__func__ , "update time in milliseconds: %ld", curr_time_msec);
}

struct event* event_init(){
    int kq;
    struct event *ev = (struct event*)mem_calloc(1, sizeof(struct event));

    if ((kq=kqueue()) == -1)
        log_error(strerror(errno));
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

    event_init_pipe(ev);
    event_init_signals(ev);

    event_update_time(ev);
    ev->heap = timer_heap_new();

    return ev;
}

void event_quit(struct event *ev) {
    log_info("start close server");
    if (ev->on_exit)
        ev->on_exit(ev->http);
    if (close(ev->kqfd) == -1)
        log_warnf(NULL, "closed kqueue fd failed: %s", strerror(errno));
    log_info("server gracefully shutting down");
    exit(0);
}

int event_dispatch(struct event *ev){
    struct timespec ts, *pts;
    ensure_events_capacity(ev);

    if (timer_size(ev->heap) > 0) {
        time_msec_t mt = timer_min(ev->heap);
        time_msec_t delta = mt - curr_time_msec;
        delta = delta > 0 ? delta : 0;
        log_debugf(__func__ , "set kevent() timeout: %ld ms", delta);

        ts.tv_sec = delta / 1000;
        ts.tv_nsec = (delta % 1000) * MILLISECOND;
        pts = &ts;
    } else {
        log_debugf(__func__ , "set kevent() timeout NULL");
        pts = NULL;
    }

    int n = kevent(ev->kqfd, NULL, 0,
                   ev->events, ev->event_size, pts);

    event_update_time(ev);
    expire_timers(ev->heap, curr_time_msec);

    if (n == -1){
        if (errno != EINTR)
            log_errorf(NULL, "call kevent() failed: %s", strerror(errno));
        else /* system call interrupted by signal, and should ignore */
            log_debug("kevent() interrupted by signal");
        return 0;
    }
    if (n == 0) // timeout
        return 0;
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

    if (is_quit) {
        event_quit(ev);
    }
    return 0;
}

int event_close_fd(struct event *ev, int fd) {
    if (ev->fd_close_len == N_FD_CLOSE || is_quit)
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

struct timer_node* event_add_timer(struct event *ev, time_msec_t msec, timer_callback cb, void *data) {
    time_msec_t expire = msec + curr_time_msec;
    return timer_add(ev->heap, expire, cb, data);
}

void event_delete_timer(struct event *ev, struct timer_node *timer) {
    timer_delete(ev->heap, timer);
}

void set_connect_cb(struct event *ev, void (*cb) (struct conn *c)) {
    ev->on_connection = cb;
}

void set_exit_cb(struct event *ev, void (*cb) (struct xhttp *http)) {
    ev->on_exit = cb;
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