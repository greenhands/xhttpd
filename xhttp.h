//
// Created by Haiwei Zhang on 2021/10/20.
//

#ifndef XHTTPD_XHTTP_H
#define XHTTPD_XHTTP_H

#include "common.h"
#include "event.h"
#include "error.h"

struct xhttp {
    struct event *ev;
};

#endif //XHTTPD_XHTTP_H
