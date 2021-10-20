//
// Created by Haiwei Zhang on 2021/10/20.
//

#ifndef XHTTPD_MEMORY_H
#define XHTTPD_MEMORY_H
#include "common.h"

void* mem_calloc(size_t count, size_t size);
void* mem_realloc(void *ptr, size_t);
void mem_free(void *ptr);

#endif //XHTTPD_MEMORY_H
