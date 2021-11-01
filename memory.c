//
// Created by Haiwei Zhang on 2021/10/20.
//

#include "memory.h"
#include "error.h"

void* mem_calloc(size_t count, size_t size){
    void *p = calloc(count, size);
    if (p == NULL)
        log_error(strerror(errno));

    return p;
}

void* mem_realloc(void *ptr, size_t new_size){
    void *p = realloc(ptr, new_size);
    if (p == NULL)
        log_error(strerror(errno));

    return p;
}

void mem_free(void *ptr){
    if (ptr)
        free(ptr);
}