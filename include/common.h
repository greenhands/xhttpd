//
// Created by Haiwei Zhang on 2021/10/20.
//

#ifndef XHTTPD_COMMON_H
#define XHTTPD_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/event.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define DEBUG 1

#define LOG_PATH                "log/http.log"

#define EMPTY_CHAR              " \t"

#define HEADER_CONTENT_TYPE     "Content-Type"
#define HEADER_CONNECTION       "Connection"

#endif //XHTTPD_COMMON_H
