//
// Created by Haiwei Zhang on 2021/10/25.
//
#include "status.h"

char* status_text(enum http_code code) {
    switch (code) {
        case HTTP_OK:
            return "200 OK";
        case HTTP_NO_CONTENT:
            return "204 No Content";
        case HTTP_NOT_MODIFIED:
            return "304 Not Modified";
        case HTTP_BAD_REQUEST:
            return "401 Bad Request";
        case HTTP_NOT_FOUND:
            return "404 Not Found";
        case HTTP_METHOD_NOT_ALLOW:
            return "405 Method Not Allowed";
        case HTTP_INTERNAL_ERROR:
            return "500 Internal Server Error";
        default:
            return "0 UNKNOWN";
    }
}

char* method_text(int method) {
    switch (method) {
        case HTTP_GET:
            return "GET";
        case HTTP_HEAD:
            return "HEAD";
        case HTTP_POST:
            return "POST";
        case HTTP_PUT:
            return "PUT";
        default:
            return "UNKNOWN";
    }
}