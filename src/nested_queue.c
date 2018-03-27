/** \file nested_queue.c
 *
 * Nested multi-producer and nested multi-consumer queue implementation with
 * static storage
 */
/* Copyright 2018 Gaurav Juvekar */
#include "nested_queue.h"

static inline void *idx_to_ptr(const NestedQueue *q, unsigned int index) {
    return (char *)q->data + (q->elem_size * index);
}

static inline unsigned int ptr_to_idx(const NestedQueue *q, const void *ptr) {
    return ((char *)ptr - (char *)q->data) / q->elem_size;
}


static void *NestedQueue_acquire(NestedQueue *         q,
                                 _Atomic unsigned int *acquire,
                                 _Atomic unsigned int *limit) {
    unsigned int src = atomic_load(acquire);
    unsigned int dst;
    do {
        if (src == atomic_load(limit)) {
            return NULL;
        } else {
            dst = (src + 1) % q->n_elems;
        }
    } while (!atomic_compare_exchange_weak(acquire, &src, dst));
    return idx_to_ptr(q, src);
}


static void NestedQueue_commit(NestedQueue *         q,
                               _Atomic unsigned int *commit,
                               _Atomic unsigned int *acquire,
                               const void *          slot_ptr) {
    unsigned int slot = ptr_to_idx(q, slot_ptr);
    if (slot != atomic_load(commit)) {
        return;
    } else {
        unsigned int orig;
        unsigned int dest;
        do {
            dest = atomic_load(acquire);
            orig = atomic_exchange(commit, dest);
        } while (orig != dest);
    }
}


void *NestedQueue_write_acquire(NestedQueue *q) {
    return NestedQueue_acquire(q, &q->write_allocated, &q->read_released);
}


void NestedQueue_write_commit(NestedQueue *q, const void *slot) {
    NestedQueue_commit(q, &q->write_committed, &q->write_allocated, slot);
}


const void *NestedQueue_read_acquire(NestedQueue *q) {
    return NestedQueue_acquire(q, &q->read_acquired, &q->write_committed);
}


void NestedQueue_read_release(NestedQueue *q, const void *slot) {
    NestedQueue_commit(q, &q->read_released, &q->read_acquired, slot);
}
