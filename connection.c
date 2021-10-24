//
// Created by Haiwei Zhang on 2021/10/21.
//

#include "connection.h"

void handle_read_write(struct event *ev, struct kevent *kev, int events) {
    struct conn *c = event_find_connection(ev, kev->ident);
    if (!c) {
        log_warn("something wrong, connection missing");
        event_del(ev, kev->ident, kev->filter);
    }
    if (events & EVENT_READ && c->on_read)
        c->on_read(c);
    if (events & EVENT_WRITE && c->on_write)
        c->on_write(c);
}

void handle_connection(struct event *ev, struct kevent *kev, int events){
    int listen_fd = kev->ident;

    int client = accept(listen_fd, (struct sockaddr*)ev->address, &ev->address_len);
    if (client == -1)
        log_error(strerror(errno));
    set_nonblocking(client);

    struct conn *c = conn_new(ev, client);
    event_add(ev, client, EVENT_READ, handle_read_write);
    if (ev->on_connection)
        ev->on_connection(c);
}

static void conn_read(struct conn *c){
    int n = 0;
    while(c->valid_p < sizeof(c->r_buf)) {
        n = recv(c->fd, c->r_buf+c->valid_p, sizeof(c->r_buf) - c->valid_p, 0);
        if (n == 0) {
            log_info("connection close by remote");
            conn_close(c);
            return;
        }
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            else {
                log_warnf(strerror(errno), "error occurs when read from socket");
                conn_close(c);
                return;
            }
        }
        c->valid_p += n;
    }

    // buff is full, remove fd from kqueue
    // TODO: add it back later
    if (c->valid_p >= sizeof(c->r_buf)) {
        event_del(c->ev, c->fd, EVENT_READ);
    }

    if (c->read_callback)
        c->read_callback(c);
}

static void conn_write(struct conn *c){
    log_debug("write available");
    if (c->write_callback)
        c->write_callback(c);
}

struct conn* conn_new(struct event *ev, int fd){
    struct conn *c = event_get_free_connection(ev);
    c->fd = fd;
    inet_ntop(AF_INET, &ev->address->sin_addr, c->remote, sizeof(c->remote));
    c->port = ntohs(ev->address->sin_port);
    c->on_read = conn_read;
    c->on_write = conn_write;

    log_infof(NULL, "new connection from %s:%d", c->remote, c->port);

    return c;
}

// TODO: important, waiting to complete
void conn_close(struct conn *c) {
    log_info("close connection by server");

    event_del(c->ev, c->fd, EVENT_READ|EVENT_WRITE);
    if (c->close_callback)
        c->close_callback(c);
    if (close(c->fd) < 0) {
        log_warnf(strerror(errno), "error occurs when close connection %s", c->remote);
    }
    conn_free(c);
}

void conn_free(struct conn *c) {
    c->pos = -1;
    c->valid_p = 0;
    c->read_p = 0;
    c->w_len = 0;
    c->w_write = 0;
}

void conn_empty_buff(struct conn *c) {
    // if buff is full put fd back to kqueue, then empty the buff
    if (c->valid_p == sizeof(c->r_buf))
        event_add(c->ev, c->fd, EVENT_READ, handle_read_write);
    c->read_p = c->valid_p = 0;
}