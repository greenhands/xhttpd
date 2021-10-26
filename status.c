//
// Created by Haiwei Zhang on 2021/10/25.
//
#include "status.h"

char* status_text(enum http_code code) {
    switch (code) {
        case HTTP_OK:
            return "OK";
        case HTTP_BAD_REQUEST:
            return "Bad Request";
        case HTTP_NOT_FOUND:
            return "Not Found";
        case HTTP_METHOD_NOT_ALLOW:
            return "Method Not Allowed";
        default:
            return "UNKNOWN";
    }
}