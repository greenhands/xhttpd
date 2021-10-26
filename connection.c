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

    // add listen fd back to kqueue
    event_add(ev, listen_fd, EVENT_READ, handle_connection);

    struct conn *c = conn_new(ev, client);
    event_add(ev, client, EVENT_READ, handle_read_write);
    if (ev->on_connection)
        ev->on_connection(c);
}

// copy data from socket buff until r_buf if full or data is empty
// return:  0 ok, r_buf is full now
//          -1 something wrong, need to close connection
//          EAGAIN data empty, need to listen for io event
static int copy_socket_buff(struct conn *c) {
    while(c->valid_p < sizeof(c->r_buf)) {
        int n = recv(c->fd, c->r_buf+c->valid_p, sizeof(c->r_buf) - c->valid_p, 0);
        if (n == 0) {
            log_info("connection close by remote");
            conn_close(c);
            return -1;
        }
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK){
                conn_listen(c, EVENT_READ);
                return EAGAIN;
            }
            else {
                log_warnf(strerror(errno), "error occurs when read from socket");
                conn_close(c);
                return -1;
            }
        }
        c->valid_p += n;
    }
    return 0;
}

static void conn_read(struct conn *c){
    if (copy_socket_buff(c) == -1)
        return;

    if (c->read_callback)
        c->read_callback(c);
}

static int conn_send_buff(struct conn *c, char *b, int buf_len) {
    // TODO: do async send
    int n = write(c->fd, b, buf_len);
    if (n == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            conn_listen(c, EVENT_WRITE);
            return 0;
        }else {
//            log_debugf(strerror(errno), "send error");
            conn_close(c);
            return -1;
        }
    }
    return n;
}

static void conn_write(struct conn *c){
    log_debug("write available");
    while (1) {
        int ret = conn_send_buff(c, "HTTP/1.0 200 OK\r\n", 17);
        if (ret <= 0)
            break;
        log_debugf(NULL, "%d bytes sent", ret);
    }

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

void conn_listen(struct conn *c, int events) {
    event_add(c->ev, c->fd, events, handle_read_write);
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
    c->fd = -1;
    c->pos = -1;
    c->valid_p = 0;
    c->read_p = 0;
    c->w_len = 0;
    c->w_write = 0;
}

// empty buff and read from socket
int conn_fulfill_buff(struct conn *c) {
    c->valid_p = c->read_p = 0;
    int ret = copy_socket_buff(c);
    if (ret == -1)
        return -1;
    return 0;
}