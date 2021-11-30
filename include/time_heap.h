//
// Created by Haiwei Zhang on 2021/11/30.
//

#ifndef XHTTPD_TIME_HEAP_H
#define XHTTPD_TIME_HEAP_H

#include "common.h"
#include "memory.h"
#include "error.h"

#define HEAP_SIZE   64

typedef long time_msec_t; /* milliseconds */

typedef void (*timer_callback) (void *data);

struct timer_node {
    void *data;
    time_msec_t expire_at;
    timer_callback cb;
};

struct timer_heap {
    struct timer_node *nodes;
    int size;
    int capacity;
};

struct timer_heap* timer_heap_new();
int timer_size(struct timer_heap *heap);
void timer_add(struct timer_heap *heap, time_msec_t expire_at, timer_callback cb, void *data);
time_msec_t timer_min(struct timer_heap *heap);
void expire_timers(struct timer_heap *heap, time_msec_t curr_msec);

#endif //XHTTPD_TIME_HEAP_H
