//
// Created by Haiwei Zhang on 2021/10/20.
//

#include "error.h"

void log_m_(int level, const char *msg){
    const char *level_str;
    switch (level) {
        case LOG_DEBUG:
            level_str = "DEBUG";
            break;
        case LOG_INFO:
            level_str = "INFO";
            break;
        case LOG_ERROR:
            level_str = "ERROR";
            break;
        default:
            level_str = "???";
            break;
    }
    fprintf(stderr, "[%s] %s\n", level_str, msg);
}

void log_v_(int level, const char *msg, const char *fmt, va_list ap){
    char buf[LOG_BUF_LEN];
    int len;

    if (fmt)
        vsnprintf(buf, sizeof(buf), fmt, ap);
    else
        buf[0] = '\0';

    if(msg){
        len = strlen(buf);
        if (len < sizeof(buf) - 3){
            snprintf(buf + len, sizeof(buf) - len, ": %s", msg);
        }
    }
    log_m_(level, buf);
}

void log_debugf(const char *msg, const char *fmt, ...){
    va_list ap;

    va_start(ap, fmt);
    log_v_(LOG_DEBUG, msg, fmt, ap);
    va_end(ap);
}

void log_infof(const char *msg, const char *fmt, ...){
    va_list ap;

    va_start(ap, fmt);
    log_v_(LOG_INFO, msg, fmt, ap);
    va_end(ap);
}

void log_warnf(const char *msg, const char *fmt, ...){
    va_list ap;

    va_start(ap, fmt);
    log_v_(LOG_WARN, msg, fmt, ap);
    va_end(ap);
}

void log_errorf(const char *msg, const char *fmt, ...){
    va_list ap;

    va_start(ap, fmt);
    log_v_(LOG_ERROR, msg, fmt, ap);
    va_end(ap);
    exit(1);
}

void log_debug(const char *msg){
    log_debugf(msg, NULL);
}

void log_info(const char *msg){
    log_infof(msg, NULL);
}

void log_warn(const char *msg){
    log_warnf(msg, NULL);
}

void log_error(const char *msg){
    log_errorf(msg, NULL);
}