//
// Created by Haiwei Zhang on 2021/10/21.
//

#ifndef XHTTPD_REQUEST_H
#define XHTTPD_REQUEST_H

#include "common.h"
#include "xhttp.h"
#include "connection.h"

#define LINE_SIZE 1024

struct request {
    struct xhttp *http;
    struct conn *c;

    char line_buf[LINE_SIZE];
    int line_end;
};

void request_new(struct conn *c);
void request_free(struct request *r);

#endif //XHTTPD_REQUEST_H
