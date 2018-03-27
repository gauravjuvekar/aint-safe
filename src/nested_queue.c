/** \file nested_queue.c
 *
 * Nested multi-producer and nested multi-consumer queue implementation with
 * static storage
 */
/* Copyright 2018 Gaurav Juvekar */
#include "nested_queue.h"


static void *NestedQueue_acquire(NestedQueue *  q,
                                 void *_Atomic *acquire,
                                 void *_Atomic *limit) {
    void *src = atomic_load(acquire);
    void *dst;
    do {
        if (src == atomic_load(limit)) {
            return NULL;
        } else {
            dst = (char *)src + q->elem_size;
            if (dst >= (void *)((char *)q->data + q->n_elems * q->elem_size)) {
                dst = q->data;
            }
        }
    } while (!atomic_compare_exchange_weak(acquire, &src, dst));
    return src;
}


static void NestedQueue_commit(void *_Atomic *commit,
                               void *_Atomic *acquire,
                               const void *   slot) {
    if (slot != atomic_load(commit)) {
        return;
    } else {
        void *orig;
        void *dest;
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
    NestedQueue_commit(&q->write_committed, &q->write_allocated, slot);
}


const void *NestedQueue_read_acquire(NestedQueue *q) {
    return NestedQueue_acquire(q, &q->read_acquired, &q->write_committed);
}


void NestedQueue_read_release(NestedQueue *q, const void *slot) {
    NestedQueue_commit(&q->read_released, &q->read_acquired, slot);
}
