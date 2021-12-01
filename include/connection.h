//
// Created by Haiwei Zhang on 2021/10/21.
//

#ifndef XHTTPD_CONNECTION_H
#define XHTTPD_CONNECTION_H

#include "common.h"
#include "event.h"
#include "request.h"
#include "util.h"
#include "time_heap.h"

#define BUFF_SIZE   4096
#define TIMEOUT     5000 /* 5s */

struct file_sender;

struct conn {
    int fd;
    struct event *ev;
    struct timer_node *timer;

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
void conn_keepalive(struct conn *c);
void conn_free(struct conn *c);

void conn_add_timeout_cb(struct conn *c, time_msec_t msec, timer_callback cb);
void conn_delete_timeout_cb(struct conn *c);

int conn_buff_fulfill(struct conn *c);
int conn_buff_flush(struct conn *c);
int conn_buff_append_data(struct conn *c, char *buf, int size);
int conn_buff_append_line(struct conn *c, char *buf, int size);
int conn_send_file(struct file_sender *f, struct conn *c);

#endif //XHTTPD_CONNECTION_H
