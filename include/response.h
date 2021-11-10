//
// Created by Haiwei Zhang on 2021/10/25.
//

#ifndef XHTTPD_RESPONSE_H
#define XHTTPD_RESPONSE_H

#include "common.h"
#include "request.h"
#include "status.h"
#include "util.h"

#define EMPTY_LINE  "\r\n"

// declaration, defined in request.h
struct request;

void response_set_header(struct request *r, char *name, char *value);
char* response_get_header(struct request *r, char *name);

void response(struct request *r);
void response_error(struct request *r, int code, char *msg);
void response_body(struct request *r, char *body, int body_len);
void response_file(struct request *r, char *filename, struct stat st);

#endif //XHTTPD_RESPONSE_H
