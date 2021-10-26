//
// Created by Haiwei Zhang on 2021/10/21.
//

#ifndef XHTTPD_REQUEST_H
#define XHTTPD_REQUEST_H

#include "common.h"
#include "xhttp.h"
#include "connection.h"
#include "response.h"
#include "status.h"

#define LINE_SIZE       1024
#define HEADER_SIZE     128

struct header {
    char *key;
    char *value;
};

struct request {
    struct xhttp *http;
    struct conn *c;

    char line_buf[LINE_SIZE];
    int line_end;

    // only for parse request
    char expect;

    // below is request params
    int method;
    char *uri;
    char *version;
    struct header headers[HEADER_SIZE];
    int header_len;
    int content_len;

    // below is response contents
    int status_code;
};

void request_new(struct conn *c);
void request_free(struct request *r);
void request_read_body(struct request *r);

#endif //XHTTPD_REQUEST_H
