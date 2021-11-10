//
// Created by Haiwei Zhang on 2021/10/21.
//

#include "request.h"

/*****************************************************/
/******  below is util functions for request   *******/
/*****************************************************/

char* request_get_header(struct request *r, char *name) {
    for (int i = 0; i < r->header_len; ++i) {
        if (strequal(r->headers[i].key, name))
            return r->headers[i].value;
    }
    return NULL;
}

/*****************************************************/
/****** below is functions for request parsing *******/
/*****************************************************/

/**
 * Consume one line from connection buff
 * @param request r
 * @param connection c
 * @return 0 when got line, otherwise the caller need to try again
 * TODO: should move this function to connection.c
 */
static int request_read_line(struct request *r, struct conn *c) {
    char ch;
    int buff_size = sizeof(r->line_buf) - 1;
    char *buf = r->line_buf;

    while (r->line_end < buff_size && c->read_p < c->valid_p) {
        ch = c->r_buf[c->read_p++];
        if (ch != '\r' && ch != '\n') {
            buf[r->line_end++] = ch;
        }

        if (c->read_p == c->valid_p) {
            if (conn_buff_fulfill(c) == -1) return -1;
        }
        if (ch == '\n') {
            buf[r->line_end++] = '\0';
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

/**
 * Discard empty line in request
 * @param r
 * @param c
 * @return  0: means empty lines end, and has more content
 *          -1: means something wrong in request data
 *          EAGAIN: means data not ready, need to call it later (by io event automatically)
 * TODO: should move this function to connection.c
 */
static int discard_empty_line(struct request *r, struct conn *c) {
    while (c->read_p < c->valid_p) {
        char ch = c->r_buf[c->read_p];

        if (ch != r->expect) {
            if (r->expect == '\r') return 0;
            else {
                conn_close(c);
                return -1;
            }
        }

        c->read_p++;
        if (r->expect == '\r')
            r->expect = '\n';
        else
            r->expect = '\r';

        // ensure we have buff space on next read, or we are listening on read event
        if (c->read_p == c->valid_p) {
            if (conn_buff_fulfill(c) == -1) return -1;
        }
    }
    return EAGAIN;
}

/**
 * Read body content from connection buff up to content-length
 * @param request r
 * @param connection c
 * @return  0: finish read all body
 *          -1: error occurs when read fd, close conneciton
 *          EAGAIN: body is not prepared, waiting for read event
 */
static int read_body_from_buff(struct request *r, struct conn *c) {
    while (c->read_p < c->valid_p) {
        int body_len = r->content_len - r->body_end;
        int buf_len = c->valid_p - c->read_p;
        if (body_len < buf_len) {
            memmove(r->request_body+r->body_end, c->r_buf+c->read_p, body_len);
            r->body_end += body_len;
            c->read_p += body_len;
            return 0;
        }else {
            memmove(r->request_body+r->body_end, c->r_buf+c->read_p, buf_len);
            r->body_end += buf_len;
            if (conn_buff_fulfill(c) == -1)
                return -1;
            if (r->body_end == r->content_len)
                return 0;
        }
    }
    return EAGAIN;
}

/**
 * Determine whether a connection is persistent,
 * following the rules in frc7230 section 6.3
 * @param request r
 * @return 1: is persistent; 0: is not persistent
 */
static int request_is_persistent_connection(struct request *r) {
    char *connection = request_get_header(r, HEADER_CONNECTION);
    int is_http_1_1 = strequal(r->version, HTTP_VER_1_1);
    if (connection == NULL)
        return is_http_1_1;
    if (strequal(connection, "close"))
        return 0;
    if (is_http_1_1 || strequal(connection, "keep-alive"))
        return 1;
    return 0;
}

static void report_request(struct request *r) {
    r->keep_alive = request_is_persistent_connection(r);
    log_debugf(__func__ , "keep-alive: %s", r->keep_alive?"true":"false");

    handle_http_request(r);
}

static void after_read_request_body(struct request *r, struct conn *c) {
    // close read callback, but listen for socket close event
    c->read_callback = NULL;
    r->expect = '\r';
    if (discard_empty_line(r, c) != EAGAIN) {
        conn_close(c);
        return;
    }
    report_request(r);
}

// read_callback
static void read_request_body(struct conn *c) {
    struct request *r = c->data;

    if (read_body_from_buff(r, c) != 0)
        return;

    log_debugf(__func__ , "request body: %s", r->request_body);
    after_read_request_body(r, c);
}

// read_callback
static void before_read_request_body(struct conn *c) {
    struct request *r = c->data;

    if (r->content_len == 0) {
        after_read_request_body(r, c);
        return;
    }
    // discard all empty lines
    if (discard_empty_line(r, c) != 0)
        return;

    c->read_callback = read_request_body;
    r->request_body = mem_calloc(r->content_len, sizeof(char) + 1);
    read_request_body(c);
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
            r->expect = '\r';
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

static void parse_request_uri(struct request *r) {
    char *uri = r->uri;

    int n_path = strcspn(uri, "?#");
    r->path = alloc_copy_nstring(uri, n_path);
    log_debugf(__func__, "[path] %s", r->path);

    if (*(uri+n_path) != '?')
        return;

    uri += n_path+1;
    int n_query = strcspn(uri, "#");
    r->query = alloc_copy_nstring(uri, n_query);
    log_debugf(__func__, "[query] %s", r->query);
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
    if (!strequal(token, HTTP_VER_1_1) && !strequal(token, HTTP_VER_1_0)) {
        r->status_code = HTTP_BAD_REQUEST;
        return -1;
    }
    r->version = alloc_copy_string(token);

    parse_request_uri(r);

    log_debugf("parse_request_line", "method: [%d], uri: [%s], version: [%s]", r->method, r->uri, r->version);

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

// do the clean work
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
    struct request *ptr = mem_realloc(http->reqs, http->req_size * sizeof(struct request));
    if (ptr != http->reqs) {
        log_debugf(__func__ , "reallocate new address, correct element pointer");
        for (int i = 0; i < pos; ++i) {
            if (ptr[i].c != NULL)
                ptr[i].c->data = (void *)&ptr[i];
        }
    }
    http->reqs= ptr;
    log_debugf(__func__ , "enlarge request size to: %d", http->req_size);
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
    r->body_end = 0;
    r->res_header_len = 0;
    r->res_header_curr = 0;
    r->res_body_len = 0;
    r->res_body_curr = 0;
    r->fs.seek = 0;

    r->status_code = HTTP_OK;

    c->data = (void*)r;
    c->read_callback = get_request_line;
    c->close_callback = on_connection_close;
}

void request_free(struct request *r) {
    r->c = NULL;
    // dealloc uri, version and headers
    mem_free_set_null(r->uri);
    mem_free_set_null(r->version);
    mem_free_set_null(r->path);
    mem_free_set_null(r->query);
    for (int i = 0; i < r->header_len; ++i) {
        mem_free_set_null(r->headers[i].key);
        mem_free_set_null(r->headers[i].value);
    }
    mem_free_set_null(r->request_body);
    for (int i = 0; i < r->res_header_len; ++i) {
        mem_free_set_null(r->res_headers[i].key);
        mem_free_set_null(r->res_headers[i].value);
    }
    mem_free_set_null(r->response_body);
    if (r->fs.fd) {
        close(r->fs.fd);
        r->fs.fd = 0;
    }
}