//
// Created by Haiwei Zhang on 2021/10/21.
//

#ifndef XHTTPD_REQUEST_H
#define XHTTPD_REQUEST_H

#include "common.h"
#include "connection.h"
#include "response.h"
#include "status.h"

#define LINE_SIZE       1024
#define HEADER_SIZE     128

struct header {
    char *key;
    char *value;
};

struct file_sender {
    int fd;
    int seek;
    long long len;
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
    char *path;
    char *query;
    struct header headers[HEADER_SIZE];
    int header_len;
    int content_len;
    int body_end;
    char *request_body;
    int keep_alive;

    // below is response contents
    int sent;
    int status_code;
    struct header res_headers[HEADER_SIZE];
    int res_header_len;
    int res_header_curr;
    int res_body_len;
    int res_body_curr;
    char *response_body;
    struct file_sender fs;
};

void request_new(struct conn *c);
void request_free(struct request *r);

char* request_get_header(struct request *r, char *name);

extern void handle_http_request(struct request *r);

#endif //XHTTPD_REQUEST_H
