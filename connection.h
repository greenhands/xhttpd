//
// Created by Haiwei Zhang on 2021/10/21.
//

#ifndef XHTTPD_CONNECTION_H
#define XHTTPD_CONNECTION_H

#include "common.h"
#include "event.h"
#include "util.h"

#define BUFF_SIZE 32

struct conn {
    int fd;
    int pos;
    struct event *ev;

    void (*on_read) (struct conn *c);
    void (*on_write) (struct conn *c);

    void *data; // for request struct
    void (*read_callback) (struct conn *c);
    void (*write_callback) (struct conn *c);
    void (*close_callback) (struct conn *c);

    char remote[INET_ADDRSTRLEN];
    int port;

    char r_buf[BUFF_SIZE];
    int valid_p;
    int read_p;

    char w_buf[BUFF_SIZE];
    int w_len;
    int w_write;
};

void handle_connection(struct event *ev, struct kevent *kev, int events);
void handle_read_write(struct event *ev, struct kevent *kev, int events);

struct conn* conn_new(struct event *ev, int fd);

void conn_listen(struct conn *c, int events);
void conn_close(struct conn *c);
void conn_free(struct conn *c);
int conn_fulfill_buff(struct conn *c);

#endif //XHTTPD_CONNECTION_H
