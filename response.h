//
// Created by Haiwei Zhang on 2021/10/25.
//

#ifndef XHTTPD_RESPONSE_H
#define XHTTPD_RESPONSE_H

#include "common.h"
#include "request.h"
#include "status.h"

// declaration, defined in request.h
struct request;

void response(struct request *r);
void response_error(struct request *r, int code);

#endif //XHTTPD_RESPONSE_H
