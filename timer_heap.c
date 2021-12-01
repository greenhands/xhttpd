//
// Created by Haiwei Zhang on 2021/11/30.
//

#include "time_heap.h"

static void heap_resize(struct timer_heap *heap) {
    heap->capacity *= 2;
    heap->nptrs = mem_realloc(heap->nptrs, (heap->capacity+1)*sizeof(struct timer_node*));
}

static struct timer_node* heap_top(struct timer_heap *heap) {
    if (heap->size > 0)
        return heap->nptrs[1];
    return NULL;
}

static int heap_parent(int i) {
    return i/2;
}

static void heap_set_node(struct timer_heap *h, int pos, struct timer_node *node) {
    if (pos > h->size) {
        log_warnf(__func__ , "node index out of heap bound");
        return;
    }
    h->nptrs[pos] = node;
    node->idx = pos;
}

static void heap_float(struct timer_heap *heap, int pos) {
    int hole = pos;
    struct timer_node *node = heap->nptrs[pos];
    while (hole > 1) {
        int parent = heap_parent(hole);
        if (node->expire_at >= heap->nptrs[parent]->expire_at) {
            break;
        }
        heap_set_node(heap, hole, heap->nptrs[parent]);
        hole = parent;
    }
    if (hole != pos)
        heap_set_node(heap, hole, node);
}

static void heap_sink(struct timer_heap *heap, int pos) {
    int left = pos * 2, hole = pos;
    struct timer_node *node = heap->nptrs[pos];
    while (left <= heap->size) {
        int small = left;
        if (left < heap->size && heap->nptrs[left+1]->expire_at < heap->nptrs[left]->expire_at)
            small = left+1;
        if (node->expire_at < heap->nptrs[small]->expire_at)
            break;
        heap_set_node(heap, hole, heap->nptrs[small]);
        hole = small;
        left = hole*2;
    }
    if (hole != pos)
        heap_set_node(heap, hole, node);
}

static void heap_pop(struct timer_heap *heap) {
    timer_delete(heap, heap->nptrs[1]);
}

struct timer_heap* timer_heap_new() {
    struct timer_heap* heap = mem_calloc(1, sizeof(struct timer_heap));
    heap->capacity = HEAP_SIZE;
    heap->size = 0;
    heap->nptrs = mem_calloc(heap->capacity + 1, sizeof(struct timer_node*));

    return heap;
}

int timer_size(struct timer_heap *heap) {
    return heap->size;
}

struct timer_node* timer_add(struct timer_heap *heap, time_msec_t expire_at, timer_callback cb, void *data) {
    if (heap->size == heap->capacity)
        heap_resize(heap);
    struct timer_node *node = mem_calloc(1, sizeof(struct timer_node));
    heap_set_node(heap, ++heap->size, node);
    node->expire_at = expire_at;
    node->cb = cb;
    node->data = data;
    heap_float(heap, heap->size);
    return node;
}

void timer_delete(struct timer_heap *heap, struct timer_node *node) {
    if (!node || node->idx > heap->size) {
        log_warnf(__func__ , "error try to delete invalid node");
        return;
    }
    int pos = node->idx;
    mem_free(node);
    if (pos < heap->size) {
        heap_set_node(heap, pos, heap->nptrs[heap->size--]);
        int parent = heap_parent(pos);
        if (parent > 0 && heap->nptrs[pos]->expire_at < heap->nptrs[parent]->expire_at)
            heap_float(heap, pos);
        else
            heap_sink(heap, pos);
    } else {
        heap->size--;
    }
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