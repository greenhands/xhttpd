//
// Created by Haiwei Zhang on 2021/10/21.
//

#ifndef XHTTPD_UTIL_H
#define XHTTPD_UTIL_H

#include "common.h"
#include "memory.h"
#include "error.h"

#define strequal(s1, s2) (strcasecmp((s1), (s2)) == 0)
#define strnequal(s1, s2, n) (strncasecmp((s1), (s2), (n)) == 0)

int set_nonblocking(int fd);

char* get_token(char *str, int *len);
char* alloc_copy_string(char *str);
char* alloc_copy_nstring(char *str, int n);

#endif //XHTTPD_UTIL_H
