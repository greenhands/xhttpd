//
// Created by Haiwei Zhang on 2021/10/21.
//

#include "connection.h"

void handle_read_write(struct event *ev, struct kevent *kev, int events) {
    struct conn *c = event_find_connection(ev, kev->ident);
    if (!c) {
        log_infof(__func__ ,"fd: %d  event: %d ignore closed connection", kev->ident, events);
        event_del(ev, kev->ident, kev->filter);
        return;
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
// return:  0 fulfill the r_buf, and may have more data in socket buff
//          -1 something wrong, need to close connection
//          EAGAIN read all data from socket buff, and no guarantee r_buf has data,
//                 need to listen for next read event
static int copy_socket_buff(struct conn *c) {
    while(c->valid_p < sizeof(c->r_buf)) {
        int n = recv(c->fd, c->r_buf+c->valid_p, sizeof(c->r_buf) - c->valid_p, 0);
        if (n == 0) {
            log_infof(__func__ ,"fd: %d connection close by remote", c->fd);
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
    log_debugf(__func__ , "read fd: %d", c->fd);
    if (copy_socket_buff(c) == -1)
        return;

    if (c->read_callback)
        c->read_callback(c);
}

// send w_buf to socket
// return:  -1 error occurs on write, connection closed
//          0 send all buff, w_buf is empty now
//          EAGAIN buff not completely sent, need to continue on next write event
int conn_buff_flush(struct conn *c) {
    // if connection is closed, send nothing
    if (c->fd <= 0)
        return -1;
    while (c->w_write < c->w_len) {
        int n = write(c->fd, c->w_buf + c->w_write, c->w_len - c->w_write);
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                conn_listen(c, EVENT_WRITE);
                return EAGAIN;
            }else {
                log_warnf(__func__, "fd: %d socket send error: %s", c->fd, strerror(errno));
                conn_close(c);
                return -1;
            }
        }
        c->w_write += n;
    }
    c->w_write = c->w_len = 0;
    return 0;
}

static void conn_write(struct conn *c){
    log_debugf(__func__ , "write fd: %d", c->fd);
    if (conn_buff_flush(c) != 0)
        return;

    if (c->write_callback)
        c->write_callback(c);
}

struct conn* conn_new(struct event *ev, int fd){
    log_debugf(__func__ , "fd: %d new connection", fd);

    struct conn *c = event_get_free_connection(ev, fd);
    inet_ntop(AF_INET, &ev->address->sin_addr, c->remote, sizeof(c->remote));
    c->port = ntohs(ev->address->sin_port);
    c->on_read = conn_read;
    c->on_write = conn_write;

    log_infof(NULL, "new connection from %s:%d, create fd: %d", c->remote, c->port, fd);

    return c;
}

void conn_listen(struct conn *c, int events) {
    log_debugf(__func__ , "fd: %d listen event: %d", c->fd, events);
    event_add(c->ev, c->fd, events, handle_read_write);
}

// TODO: important, waiting to complete
void conn_close(struct conn *c) {
    log_infof(__func__ ,"fd: %d close connection by server", c->fd);

    event_del(c->ev, c->fd, EVENT_READ|EVENT_WRITE);
    if (c->close_callback)
        c->close_callback(c);
    if (event_close_fd(c->ev, c->fd) < 0) {
        log_warnf(strerror(errno), "fd: %d error occurs when close connection %s", c->fd, c->remote);
    }
    conn_free(c);
}

void conn_free(struct conn *c) {
    c->fd = -1;
    c->valid_p = 0;
    c->read_p = 0;
    c->w_len = 0;
    c->w_write = 0;

    c->read_callback = NULL;
    c->write_callback = NULL;
    c->close_callback = NULL;
}

// empty buff and read from socket
int conn_fulfill_buff(struct conn *c) {
    c->valid_p = c->read_p = 0;
    int ret = copy_socket_buff(c);
    if (ret == -1)
        return -1;
    return 0;
}

// return size of actual appended buff
//        -1 when error occurs
int conn_buff_append_data(struct conn *c, char *buf, int size) {
    if (c->w_write == c->w_len)
        c->w_write = c->w_len = 0;

    int send_n = 0, left_n = size;
    while (send_n < size) {
        if (c->w_len + left_n > BUFF_SIZE) {
            int free = BUFF_SIZE - c->w_len;
            memmove(c->w_buf+c->w_len, buf + send_n, free);
            c->w_len = BUFF_SIZE;
            send_n += free;
            left_n -= free;

            int ret = conn_buff_flush(c);
            if (ret == -1)
                return -1;
            if (ret == EAGAIN)
                return send_n;
        }else {
            memmove(c->w_buf+c->w_len, buf + send_n, left_n);
            c->w_len += left_n;
            return size;
        }
    }
    return size;
}

// append line which can not be truncated
// return   0 append success
//          -1 line size too large or error
//          EAGAIN currently has no space for line
int conn_buff_append_line(struct conn *c, char *buf, int size) {
    if (c->w_write == c->w_len)
        c->w_write = c->w_len = 0;

    if (size > BUFF_SIZE) {
        conn_close(c);
        return -1;
    }
    if (c->w_len + size > BUFF_SIZE) {
        // buff space not enough, try to send buff
        int ret = conn_buff_flush(c);
        if (ret != 0)
            return ret;
    }

    memmove(c->w_buf+c->w_len, buf, size);
    c->w_len += size;
    return 0;
}

// send file to socket use sendfile()
// return   0 finish
//          -1 error occurs and close connection
//          EAGAIN to be continued
int conn_send_file(struct file_sender *f, struct conn *c) {
    int ret = sendfile(f->fd, c->fd, f->seek, &f->len, NULL, 0);
    if (ret == -1) {
        if (errno == EAGAIN || errno == EINTR) {
            goto again;
        } else{
            conn_close(c);
            return -1;
        }
    }
    if (f->len == 0) {
        return 0;
    }
again:
    f->seek += f->len;
    conn_listen(c, EVENT_WRITE);
    return EAGAIN;
}