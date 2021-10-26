//
// Created by Haiwei Zhang on 2021/10/25.
//

#ifndef XHTTPD_STATUS_H
#define XHTTPD_STATUS_H

enum http_code {
    HTTP_OK                 = 200,
    HTTP_BAD_REQUEST        = 401,
    HTTP_NOT_FOUND          = 404,
    HTTP_METHOD_NOT_ALLOW   = 405, // should return allowed methods
};

#define HTTP_GET        (1<<0)
#define HTTP_POST       (1<<1)

char* status_text(enum http_code code);

#endif //XHTTPD_STATUS_H
