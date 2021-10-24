//
// Created by Haiwei Zhang on 2021/10/21.
//

#include "request.h"

// consume one line from connection buf
// return: 0 when got line, otherwise the caller need to try again
static int request_read_line(struct request *r, struct conn *c) {
    char ch;
    int buff_size = sizeof(r->line_buf) - 1;
    char *buf = r->line_buf;

    while (r->line_end < buff_size && c->read_p < c->valid_p) {
        ch = c->r_buf[c->read_p++];
        if (ch != '\r') {
            buf[r->line_end++] = ch;
        }

        if (c->read_p == c->valid_p) {
            conn_empty_buff(c);
        }
        if (ch == '\n') {
            buf[r->line_end-1] = '\0';
            return 0;
        }
    }
    if (r->line_end == buff_size) {
        conn_close(c);
        log_warn("request line is too long");
        return -1;
    }
    return EAGAIN;
}

static void report_request(struct request *r) {
    if (r->http->http_handler)
        r->http->http_handler(r);
}

// discard empty line
// return: 0 means empty lines end
//         -1 means something wrong in request data
//         EAGAIN means discard one empty line, and need to call this function again
static int discard_empty_line(struct request *r, struct conn *c) {
    if (r->content_len == 0) {
        c->read_callback = NULL;
        return 0;
    }
    int buf_len = c->valid_p - c->read_p;
    if (buf_len == 0)
        return EAGAIN;

    if (c->r_buf[c->read_p] != '\r')
        return 0;

    if (buf_len == 1)
        return EAGAIN;

    if (c->r_buf[c->read_p+1] != '\n')
        return -1;

    c->read_p += 2;
    return EAGAIN;
}

// read_callback
static void before_read_request_body(struct conn *c) {
    struct request *r = c->data;

    // TODO: discard all empty lines

    report_request(r);
    log_debug("before_read_request_body");
}

static int check_request_header(struct request *r, struct header *h) {
    if (strequal(h->key, "Content-Length")) {
        int len = strtol(h->value, NULL, 10);
        if (len == 0) {
            log_warnf(strerror(errno), "request header Content-Length invalid");
            return -1;
        }
        r->content_len = len;
    }
    return 0;
}

static int parse_request_header(struct request *r) {
    char *key, *value, *line;

    if (r->header_len == HEADER_SIZE) {
        log_warnf(NULL, "header discard: %s", r->line_buf);
        return 0;
    }
    // key
    key = line = r->line_buf;
    char *d = strpbrk(line, ":");
    if (!d || d == line || d[1] == '\0') {
        r->status_code = HTTP_BAD_REQUEST;
        return -1;
    }
    *d = '\0';

    // value
    line = d+1;
    value = line + strspn(line, EMPTY_CHAR);
    if (*value == '\0') {
        r->status_code = HTTP_BAD_REQUEST;
        return -1;
    }

    struct header *h = &r->headers[r->header_len++];
    h->key = alloc_copy_string(key);
    h->value = alloc_copy_string(value);
    if (check_request_header(r, h) != 0) {
        r->status_code = HTTP_BAD_REQUEST;
        return -1;
    }

    log_debugf("parse_request_header", "header: key: [%s], value: [%s]", key, value);
    return 0;
}

// read_callback
static void get_request_headers(struct conn *c) {
    struct request *r = c->data;

    while (1) {
        if (request_read_line(r, c) != 0)
            return;

        if (DEBUG)
            log_debugf("get_request_headers", "request header: %s", r->line_buf);

        if (r->line_buf[0] == '\0') {
            r->line_end = 0;
            c->read_callback = before_read_request_body; // read header finish, next to read request body (if any)
            before_read_request_body(c);
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

    log_debugf(NULL, "method: [%d], uri: [%s], version: [%s]", r->method, r->uri, r->version);

    return 0;
}

// read_callback
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
    r->content_len = 0;

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

    log_debugf(NULL, "status code: %d", r->status_code);
    // TODO: write http headers and body
    conn_close(r->c);
}