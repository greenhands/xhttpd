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

#define XHTTP_LISTEN_PORT 5000

#define N_REQUEST 64

struct xhttp {
    struct event *ev;
    int allowed_methods;

    struct request *reqs;
    int req_size;
};

void xhttp_init(struct xhttp *http);
void xhttp_start(struct xhttp *http);

#endif //XHTTPD_XHTTP_H
