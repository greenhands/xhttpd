//
// Created by Haiwei Zhang on 2021/10/20.
//

#ifndef XHTTPD_XHTTP_H
#define XHTTPD_XHTTP_H

#include "common.h"
#include "event.h"
#include "error.h"
#include "request.h"
#include "memory.h"
#include "util.h"

#define XHTTP_LISTEN_PORT 20022

#define N_REQUEST   1024
#define N_HANDLER   1024

#define DOC_ROOT    "www"
#define DOC_INDEX   "index.html"

#define HTTP_VER_1_0    "HTTP/1.0"
#define HTTP_VER_1_1    "HTTP/1.1"

struct request;

typedef void (*http_handler) (struct request *r);

struct handler {
    char *pattern;
    http_handler func;
};

struct xhttp {
    struct event *ev;
    int listen_fd;
    int allowed_methods;

    struct request *reqs;
    int req_size;

    struct handler *handlers;
    int handle_size;
};

void xhttp_init(struct xhttp *http);
void xhttp_start(struct xhttp *http);

void xhttp_set_handler(struct xhttp *http, char *pattern, http_handler f);

#endif //XHTTPD_XHTTP_H
