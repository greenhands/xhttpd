//
// Created by Haiwei Zhang on 2021/10/21.
//

#include "request.h"

static int request_read_line(struct request *r, struct conn *c) {
    int n = 0;
    char ch;
    int buff_size = sizeof(r->line_buf) - 1 - r->line_end;
    char *buf = r->line_buf + r->line_end;

    while (n < buff_size && c->read_p < c->valid_p) {
        ch = c->r_buf[c->read_p];
        if (ch != '\r') {
            buf[n] = ch;
        }
        n++;c->read_p++;r->line_end++;

        if (c->read_p == c->valid_p) {
            conn_empty_buff(c);
        }
        if (ch == '\n') {
            buf[n] = '\0';
            if (DEBUG)
                log_debugf(NULL, "%d characters read, vp: %d, rp: %d", n, c->valid_p, c->read_p);
            return 0;
        }
    }
    if (n == buff_size) {
        conn_close(c);
        log_warn("request line is too long");
        return -1;
    }
    return EAGAIN;
}

static void get_request_headers(struct conn *c) {
    struct request *r = c->data;
}

static int parse_request_line(struct request *r) {
    char *p = r->line_buf;
    // TODO
}

static void get_request_line(struct conn *c) {
    struct request *r = c->data;
    if (request_read_line(r, c) != 0) {
        return;
    }
    if (DEBUG) {
        log_debugf(r->line_buf, "length %d", r->line_end);
    }

    r->line_end = 0;
    c->read_callback = get_request_headers; // next to read request headers
}

static void on_connection_close(struct conn *c) {
    struct request *r = c->data;
    request_free(r);
}

static struct request* get_free_request(struct xhttp *http) {
    for (int i = 0; i < http->req_size; ++i) {
        if (http->reqs[i].c == NULL)
            return &http->reqs[i];
    }
    int pos = http->req_size;
    http->req_size *= 2;
    http->reqs = mem_realloc(http->reqs, http->req_size * sizeof(struct request));
    return &http->reqs[pos];
}

void request_new(struct conn *c) {
    struct xhttp *http = c->ev->http;
    struct request *r = get_free_request(http);

    r->http = http;
    r->c = c;
    r->line_end = 0;

    c->data = (void*)r;
    c->read_callback = get_request_line;
    c->close_callback = on_connection_close;
}

void request_free(struct request *r) {
    r->line_end = 0;
    r->c = NULL;
}