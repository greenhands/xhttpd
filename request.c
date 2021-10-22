//
// Created by Haiwei Zhang on 2021/10/21.
//

#include "request.h"

// consume one line from connection buf
// return 0 when got line, otherwise the caller need to try again
static int request_read_line(struct request *r, struct conn *c) {
    int n = 0;
    char ch;
    int buff_size = sizeof(r->line_buf) - 1 - r->line_end;
    char *buf = r->line_buf + r->line_end;

    while (n < buff_size && c->read_p < c->valid_p) {
        ch = c->r_buf[c->read_p++];
        if (ch != '\r') {
            buf[n] = ch;
            n++;r->line_end++;
        }

        if (c->read_p == c->valid_p) {
            conn_empty_buff(c);
        }
        if (ch == '\n') {
            buf[n] = '\0';
            if (DEBUG) {
                log_debugf("request_read_line", "read from buff: %s[%d] total: %d, curr: %d", buf, n, c->valid_p, c->read_p);
            }
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

static void get_request_body(struct conn *c) {
    // TODO: discard all empty lines
    log_debug("get_request_body");
}

static int parse_request_header(struct request *r) {
    log_debugf("parse_request_header", "%s", r->line_buf);
    return 0;
}

static void get_request_headers(struct conn *c) {
    struct request *r = c->data;

    while (1) {
        if (request_read_line(r, c) != 0)
            return;

        if (DEBUG)
            log_debugf("get_request_headers", "request header: %s, buff_len: %d", r->line_buf, r->line_end);

        if (r->line_buf[0] == '\n') {
            r->line_end = 0;
            c->read_callback = get_request_body; // read header finish, next to read request body (if any)
            get_request_body(c);
            return;
        }

        if (parse_request_header(r) == -1) {
            response(r);
            return;
        }

        r->line_end = 0;
    }
}

static int parse_request_line(struct request *r) {
    char *token, *line = r->line_buf;
    int len;
    // method
    token = get_token(line, &len);
    line = token + len;
    if (!token) {
        r->status_code = HTTP_BAD_REQUEST;
        return -1;
    }
    if (strnequal(token, "GET", len)) r->method = HTTP_GET;
    else if (strnequal(token, "POST", len)) r->method = HTTP_POST;
    else {
        r->status_code = HTTP_METHOD_NOT_ALLOW;
        return -1;
    }

    // uri
    token = get_token(line, &len);
    line = token + len;
    if (!token || *line == '\0') {
        r->status_code = HTTP_BAD_REQUEST;
        return -1;
    }
    *line++ = '\0';
    r->uri = alloc_copy_string(token);

    // version
    token = get_token(line, &len);
    if (!token) {
        r->status_code = HTTP_BAD_REQUEST;
        return -1;
    }
    if (!strequal(token, "HTTP/1.1")) {
        r->status_code = HTTP_BAD_REQUEST;
        return -1;
    }
    r->version = alloc_copy_string(token);

    log_debugf(NULL, "request line parse finish, method: %d, uri: %s, version: %s", r->method, r->uri, r->version);

    return 0;
}

static void get_request_line(struct conn *c) {
    struct request *r = c->data;
    if (request_read_line(r, c) != 0)
        return;

    if (DEBUG) {
        log_debugf("get_request_line", "request line: %s", r->line_buf);
    }

    if (parse_request_line(r) == -1) {
        response(r);
        return;
    }

    if (!(r->method & r->http->allowed_methods)) {
        r->status_code = HTTP_METHOD_NOT_ALLOW;
        response(r);
        return;
    }

    r->line_end = 0; // empty line buff for next read
    c->read_callback = get_request_headers; // next to read request headers
    get_request_headers(c);
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
    r->header_len = 0;

    r->status_code = HTTP_OK;

    c->data = (void*)r;
    c->read_callback = get_request_line;
    c->close_callback = on_connection_close;
}

void request_free(struct request *r) {
    r->c = NULL;
    // dealloc uri, version and headers
    mem_free(r->uri);
    mem_free(r->version);
    for (int i = 0; i < r->header_len; ++i) {
        mem_free(r->headers[i].key);
        mem_free(r->headers[i].value);
    }
}

static void write_response_line(struct request *r) {

}

static void write_response_headers(struct request *r) {

}

void response(struct request *r) {
    // close read callback
    r->c->read_callback = NULL;

    // TODO: write http headers and body
    // TODO: close connection when finish
    conn_close(r->c);
}