/** \file nested_queue.c
 *
 * Nested multi-producer and nested multi-consumer queue implementation with
 * static storage
 */
/* Copyright 2018 Gaurav Juvekar */
#include "nested_queue.h"
#include <string.h>
#include <assert.h>

static inline void *idx_to_ptr(const NestedQueue *q, unsigned int index) {
    return (char *)q->data + (q->elem_size * index);
}

static inline unsigned int ptr_to_idx(const NestedQueue *q, const void *ptr) {
    return ((char *)ptr - (char *)q->data) / q->elem_size;
}


static void *
NestedQueue_acquire(NestedQueue *q, int count_idx, int acquire_idx) {
    mcas_base_t old_indexes[NESTED_QUEUE_NUMBER_OF_INDEXES];
    mcas_base_t new_indexes[NESTED_QUEUE_NUMBER_OF_INDEXES];
    do {
        Mcas_read(&q->indexes, old_indexes);
        if (!old_indexes[count_idx]) { return NULL; }

        memcpy(new_indexes, old_indexes, sizeof(new_indexes));
        new_indexes[acquire_idx] = (new_indexes[acquire_idx] + 1) % q->n_elems;
        new_indexes[count_idx] -= 1;
    } while (!Mcas_compare_exchange(&q->indexes, old_indexes, new_indexes));

    return idx_to_ptr(q, old_indexes[acquire_idx]);
}


static void NestedQueue_commit(NestedQueue *q,
                               int          commit_idx,
                               int          acquire_idx,
                               int          count_idx,
                               const void * slot_ptr,
                               NestedQueueOperationOrder order) {
    mcas_base_t idx = ptr_to_idx(q, slot_ptr);
    mcas_base_t old_indexes[NESTED_QUEUE_NUMBER_OF_INDEXES];
    mcas_base_t new_indexes[NESTED_QUEUE_NUMBER_OF_INDEXES];

    switch (order) {
    case NESTED_QUEUE_OPERATION_ORDER_NESTED:
        do {
            Mcas_read(&q->indexes, old_indexes);
            if (old_indexes[commit_idx] != idx) { return; }

            memcpy(new_indexes, old_indexes, sizeof(new_indexes));
            new_indexes[commit_idx] = old_indexes[acquire_idx];
            new_indexes[count_idx] += (old_indexes[acquire_idx] + q->n_elems
                                       - new_indexes[commit_idx])
                                      % q->n_elems;
        } while (
                !Mcas_compare_exchange(&q->indexes, old_indexes, new_indexes));
        break;

    case NESTED_QUEUE_OPERATION_ORDER_FCFS:
        do {
            Mcas_read(&q->indexes, old_indexes);
            assert(old_indexes[commit_idx] == idx);

            memcpy(new_indexes, old_indexes, sizeof(new_indexes));
            new_indexes[commit_idx] += 1;
            new_indexes[commit_idx] %= q->n_elems;
            new_indexes[count_idx] += 1;
        } while (
                !Mcas_compare_exchange(&q->indexes, old_indexes, new_indexes));
    }
}


void *NestedQueue_write_acquire(NestedQueue *q) {
    return NestedQueue_acquire(
            q, NESTED_QUEUE_COUNT_WRITABLE, NESTED_QUEUE_WRITE_ALLOCATED);
}


void NestedQueue_write_commit(NestedQueue *q, const void *slot) {
    NestedQueue_commit(q,
                       NESTED_QUEUE_WRITE_COMMITTED,
                       NESTED_QUEUE_WRITE_ALLOCATED,
                       NESTED_QUEUE_COUNT_READABLE,
                       slot,
                       q->write_order);
}


const void *NestedQueue_read_acquire(NestedQueue *q) {
    return NestedQueue_acquire(
            q, NESTED_QUEUE_COUNT_READABLE, NESTED_QUEUE_READ_ACQUIRED);
}


void NestedQueue_read_release(NestedQueue *q, const void *slot) {
    NestedQueue_commit(q,
                       NESTED_QUEUE_READ_RELEASED,
                       NESTED_QUEUE_READ_ACQUIRED,
                       NESTED_QUEUE_COUNT_WRITABLE,
                       slot,
                       q->read_order);
}


NestedQueueIterator NestedQueueIterator_init_read(NestedQueue *q) {
    mcas_base_t indexes[NESTED_QUEUE_NUMBER_OF_INDEXES];
    Mcas_read(&q->indexes, indexes);
    return (NestedQueueIterator){
            .queue     = q,
            .current_i = indexes[NESTED_QUEUE_READ_RELEASED],
            .end_i     = indexes[NESTED_QUEUE_READ_ACQUIRED],
    };
}


NestedQueueIterator NestedQueueIterator_init_write(NestedQueue *q) {
    mcas_base_t indexes[NESTED_QUEUE_NUMBER_OF_INDEXES];
    Mcas_read(&q->indexes, indexes);
    return (NestedQueueIterator){
            .queue     = q,
            .current_i = indexes[NESTED_QUEUE_WRITE_COMMITTED],
            .end_i     = indexes[NESTED_QUEUE_WRITE_ALLOCATED],
    };
}


void *NestedQueueIterator_next(NestedQueueIterator *iterator) {
    if (iterator->current_i != iterator->end_i) {
        void *ret = idx_to_ptr(iterator->queue, iterator->current_i);
        iterator->current_i += 1;
        iterator->current_i %= iterator->queue->n_elems;
        return ret;
    }
    else {
        return NULL;
    }
}
