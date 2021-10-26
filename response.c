//
// Created by Haiwei Zhang on 2021/10/25.
//
#include "response.h"

static void write_response_line(struct request *r) {

}

static void write_response_headers(struct request *r) {

}

void response(struct request *r) {
    if (r->status_code != HTTP_OK) {
        response_error(r, r->status_code);
        return;
    }
    // close read callback
    r->c->read_callback = NULL;
    // TODO: send headers
    conn_close(r->c);
}

void response_error(struct request *r, int code) {
    r->c->read_callback = NULL;
    // TODO
    conn_close(r->c);
}