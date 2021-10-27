//
// Created by Haiwei Zhang on 2021/10/25.
//
#include "response.h"

static void finish_write_response(struct conn *c) {
    log_debug(__func__ );

    struct request *r = c->data;

    if (conn_buff_flush(c) != 0)
        return;
    conn_close(c);
}

static void write_response_body(struct conn *c) {
    log_debug(__func__ );

    struct request *r = c->data;
    char *body = "1234567890";

    int ret = conn_buff_append_data(c, body, 10);
    if (ret == -1)
        return;
    if (ret < 10)
        return;

    c->write_callback = finish_write_response;
    finish_write_response(c);
}

static void write_response_headers(struct conn *c) {
    log_debug(__func__ );

    struct request *r = c->data;
    char *line = "Content-Type: text/plain; charset=utf-8\r\nContent-Length: 10\r\n\r\n";
    if (conn_buff_append_line(c, line, strlen(line)) != 0)
        return;

    c->write_callback = write_response_body;
    write_response_body(c);
}

static void write_response_line(struct conn *c) {
    log_debug(__func__ );

    struct request *r = c->data;
    char *line = "HTTP/1.0 200 OK\r\n";
    if (conn_buff_append_line(c, line, strlen(line)) != 0)
        return;

    c->write_callback = write_response_headers;
    write_response_headers(c);
}

void response(struct request *r) {
    if (r->status_code != HTTP_OK) {
        response_error(r, r->status_code);
        return;
    }
    // close read callback
    r->c->read_callback = NULL;
    r->c->write_callback = write_response_line;
    conn_listen(r->c, EVENT_WRITE);
}

void response_error(struct request *r, int code) {
    r->c->read_callback = NULL;
    // TODO
    conn_close(r->c);
}