//
// Created by Haiwei Zhang on 2021/11/30.
//

#include "time_heap.h"

static void heap_resize(struct timer_heap *heap) {
    heap->capacity *= 2;
    heap->nodes = mem_realloc(heap->nodes, (heap->capacity+1)* sizeof(struct timer_heap));
}

static struct timer_node* heap_top(struct timer_heap *heap) {
    if (heap->size > 0)
        return &heap->nodes[1];
    return NULL;
}

static int heap_parent(int i) {
    return i/2;
}

static void heap_float(struct timer_heap *heap, int pos) {
    int hole = pos;
    struct timer_node node = heap->nodes[pos];
    while (hole > 1) {
        int parent = heap_parent(hole);
        if (node.expire_at >= heap->nodes[parent].expire_at) {
            break;
        }
        heap->nodes[hole] = heap->nodes[parent];
        hole = parent;
    }
    if (hole != pos)
        heap->nodes[hole] = node;
}

static void heap_sink(struct timer_heap *heap, int pos) {
    int left = pos * 2, hole = pos;
    struct timer_node node = heap->nodes[pos];
    while (left <= heap->size) {
        int small = left;
        if (left < heap->size && heap->nodes[left+1].expire_at < heap->nodes[left].expire_at)
            small = left+1;
        if (node.expire_at < heap->nodes[small].expire_at)
            break;
        heap->nodes[hole] = heap->nodes[small];
        hole = small;
        left = hole*2;
    }
    if (hole != pos)
        heap->nodes[hole] = node;
}

static void heap_pop(struct timer_heap *heap) {
    heap->nodes[1] = heap->nodes[heap->size--];
    heap_sink(heap, 1);
}

struct timer_heap* timer_heap_new() {
    struct timer_heap* heap = mem_calloc(1, sizeof(struct timer_heap));
    heap->capacity = HEAP_SIZE;
    heap->size = 0;
    heap->nodes = mem_calloc(heap->capacity + 1, sizeof(struct timer_heap));

    return heap;
}

int timer_size(struct timer_heap *heap) {
    return heap->size;
}

void timer_add(struct timer_heap *heap, time_msec_t expire_at, timer_callback cb, void *data) {
    if (heap->size == heap->capacity)
        heap_resize(heap);
    heap->size++;
    heap->nodes[heap->size].expire_at = expire_at;
    heap->nodes[heap->size].cb = cb;
    heap->nodes[heap->size].data = data;
    heap_float(heap, heap->size);
}

time_msec_t timer_min(struct timer_heap *heap) {
    struct timer_node *top = heap_top(heap);
    if (top)
        return top->expire_at;
    return 0;
}

void expire_timers(struct timer_heap *heap, time_msec_t curr_msec) {
    while (timer_size(heap) > 0) {
        struct timer_node *node = heap_top(heap);
        if (node->expire_at > curr_msec)
            return;
        if (node->cb)
            node->cb(node->data);
        heap_pop(heap);
    }
}