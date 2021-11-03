//
// Created by Haiwei Zhang on 2021/10/25.
//
#include "status.h"

char* status_text(enum http_code code) {
    switch (code) {
        case HTTP_OK:
            return "200 OK";
        case HTTP_BAD_REQUEST:
            return "401 Bad Request";
        case HTTP_NOT_FOUND:
            return "404 Not Found";
        case HTTP_METHOD_NOT_ALLOW:
            return "405 Method Not Allowed";
        default:
            return "0 UNKNOWN";
    }
}