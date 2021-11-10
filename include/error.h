//
// Created by Haiwei Zhang on 2021/10/20.
//

#ifndef XHTTPD_ERROR_H
#define XHTTPD_ERROR_H

#include "common.h"

#define OK 0
#define FAIL 1

#define LOG_DEBUG 0
#define LOG_INFO 1
#define LOG_WARN 2
#define LOG_ERROR 3

#define LOG_BUF_LEN 1024

void log_debugf(const char *msg, const char *fmt, ...);
void log_infof(const char *msg, const char *fmt, ...);
void log_warnf(const char *msg, const char *fmt, ...);
void log_errorf(const char *msg, const char *fmt, ...);
void log_debug(const char *msg);
void log_info(const char *msg);
void log_warn(const char *msg);
void log_error(const char *msg);

#endif //XHTTPD_ERROR_H
