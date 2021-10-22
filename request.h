//
// Created by Haiwei Zhang on 2021/10/21.
//

#ifndef XHTTPD_REQUEST_H
#define XHTTPD_REQUEST_H

#include "common.h"
#include "xhttp.h"
#include "connection.h"

#define LINE_SIZE       1024
#define HEADER_SIZE     128

enum http_code {
    HTTP_OK                 = 200,
    HTTP_BAD_REQUEST        = 401,
    HTTP_NOT_FOUND          = 404,
    HTTP_METHOD_NOT_ALLOW   = 405, // should return allowed methodss
};

#define HTTP_GET        (1<<0)
#define HTTP_POST       (1<<1)

struct header {
    char *key;
    char *value;
};

struct request {
    struct xhttp *http;
    struct conn *c;

    char line_buf[LINE_SIZE];
    int line_end;

    // below is request params
    int method;
    char *uri;
    char *version;
    struct header headers[HEADER_SIZE];
    int header_len;

    // below is response contents
    int status_code;
};

void request_new(struct conn *c);
void request_free(struct request *r);

void response(struct request *r);

#endif //XHTTPD_REQUEST_H
