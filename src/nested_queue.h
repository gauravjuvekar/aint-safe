/** \file nested_queue.h
 *
 * Nested multi-producer and nested multi-consumer queue implementation with
 * static storage
 *
 * Usage:
 * \code{.c}
 * \endcode
 */
/* Copyright 2018 Gaurav Juvekar */

#ifndef AINT_SAFE__NESTED_QUEUE_H
#define AINT_SAFE__NESTED_QUEUE_H 1
#include <stdatomic.h>
#include <stddef.h>

#include "mcas.h"

#if !defined(__DOXYGEN__AINT_SAFE__)
_Static_assert(
        ATOMIC_INT_LOCK_FREE,
        "Your stdlib implementation does not have lock-free int atomics");
#endif


typedef enum {
    /** Index in indexes of next slot in data that can be acquired for writing */
    NESTED_QUEUE_WRITE_ALLOCATED,
    /** Index in indexes of first slot in data that is being written to */
    NESTED_QUEUE_WRITE_COMMITTED,
    /** Index in indexes of  next slot in data that can be acquired for reading */
    NESTED_QUEUE_READ_ACQUIRED,
    /** Index in indexes of oldest slot in data that is being read */
    NESTED_QUEUE_READ_RELEASED,
    /** Index in indexes of count of writable slots left */
    NESTED_QUEUE_COUNT_WRITABLE,
    /** Index in indexes of count of readable slots available */
    NESTED_QUEUE_COUNT_READABLE,
    /** Number of elements in the indexes array */
    NESTED_QUEUE_NUMBER_OF_INDEXES,
} NestedQueueIndexes;


typedef enum {
    /** All acquires must be followed by the corresponding release after any
     * other acquire-release pairs
     */
    NESTED_QUEUE_OPERATION_ORDER_NESTED,
    /** Order of releases must be the same as order of acquires
     * This is suitable when there is only one user
     *
     * \warning Do not use this in cases  of multiple producer/consumers unless
     * you really know what you are doing.
     */
    NESTED_QUEUE_OPERATION_ORDER_FCFS,
} NestedQueueOperationOrder;


/** \brief Internal data structure of the nested MPMC queue
 *
 * This must be initialized with #NESTED_QUEUE_STATIC_INIT at declaration
 */
typedef struct NestedQueue {
    _Atomic mcas_base_t index_stoarge_[NESTED_QUEUE_NUMBER_OF_INDEXES];
    Mcas indexes;
    /** Data to allocate slots from */
    void *const data;
    /** Number of elements in #data */
    const size_t n_elems;
    /** Size of a slot in #data */
    const size_t elem_size;
    /** The ordering used for read operations */
    const NestedQueueOperationOrder read_order;
    /** The ordering used for write operations */
    const NestedQueueOperationOrder write_order;
} NestedQueue;


/** \brief Statically initialize a #NestedQueue
 *
 * Use this macro to initialize a #NestedQueue at definition.
 *
 * \note You need to \e tentatively \e define (search what a C tentative
 * definition is) the #NestedQueue first.
 *
 * \param p_nested_queue the \e tentatively \e defined #NestedQueue to
 *                       initialize
 * \param p_elem_size    size of one element of \p data
 * \param p_n_elems      number of elements in \p data
 * \param p_data_array   data array to allocate from
 * \param p_write_order  ordering of acquire and release that will be used for
 *                       writes
 * \param p_read_order   ordering of acquire and release that will be used for
 *                       reads
 *
 * \return A #NestedQueue static initialiizer
 *
 * \code{.c}
 * static int mydata[10];
 * static NestedQueue the_queue;
 * static NestedQueue the_queue = NESTED_QUEUE_STATIC_INIT(
 *         the_queue, sizeof(mydata[0]), 10, mydata);
 *
 * \endcode
 */
#define NESTED_QUEUE_STATIC_INIT(p_nested_queue,                              \
                                 p_elem_size,                                 \
                                 p_n_elems,                                   \
                                 p_data_array,                                \
                                 p_write_order,                               \
                                 p_read_order)                                \
    {                                                                         \
        .data = p_data_array, .n_elems = p_n_elems, .elem_size = p_elem_size, \
        .index_storage = {[NESTED_QUEUE_WRITE_ALLOCATED] = 0,                 \
                          [NESTED_QUEUE_WRITE_COMMITTED] = 0,                 \
                          [NESTED_QUEUE_READ_ACQUIRED]   = 0,                 \
                          [NESTED_QUEUE_READ_RELEASED]   = 0,                 \
                          [NESTED_QUEUE_COUNT_READABLE]  = 0,                 \
                          [NESTED_QUEUE_COUNT_WRITABLE]  = p_n_elems},        \
        .indexes       = MCAS_STATIC_INIT(NESTED_QUEUE_NUMBER_OF_INDEXES,     \
                                    &p_nested_queue.index_storage_),          \
        .read_order = p_read_order, .write_order = p_write_order              \
    }


/** \brief Acquire an available slot from the queue for writing
 *
 * \param q #NestedQueue to acquire the slot from
 *
 * \return Pointer to an available slot in \p q->data
 * \retval NULL if no slot is available in \p q->data
 *
 * \pre \p q must be initialized with #NESTED_QUEUE_STATIC_INIT
 * \post #NestedQueue_write_commit() must be called after writing to the
 * returned slot.
 */
void *NestedQueue_write_acquire(NestedQueue *q);


/** \brief Commit a slot acquired for writing
 *
 * \param q    #NestedQueue to from which \p slot was acquired
 * \param slot slot acquired by #NestedQueue_write_acquire()
 *
 * \note Calls must follow \p q->write_order
 */
void NestedQueue_write_commit(NestedQueue *q, const void *slot);


/** \brief Acquire a slot from the queue for reading
 *
 * \param q #NestedQueue to acquire the slot from
 *
 * \return Pointer to an available slot in \p q->data for reading
 * \retval NULL if no slot is available in \p q->data for reading
 *
 * \pre \p q must be initialized with #NESTED_QUEUE_STATIC_INIT
 * \post #NestedQueue_read_release() must be called after using the slot
 */
const void *NestedQueue_read_acquire(NestedQueue *q);


/** \brief Release a slot acquired for reading and release its memory
 *
 * \param q    #NestedQueue to from which \p slot was acquired
 * \param slot slot acquired by #NestedQueue_read_acquire()
 *
 * \note Calls must follow \p q->read_order
 */
void NestedQueue_read_release(NestedQueue *q, const void *slot);


/** \brief Iterate over acquired (read/write) regions
 *
 * Must be initialized with #NestedQueueIterator_init_read or
 * #NestedQueueIterator_init_write.
 * \note This iterator iterates over the \b entire acquired region (at the time
 * of initialization), irrespective of who acquired it. It is your
 * responsibility to ensure mutual exclusion if iterating over a region that
 * may be partly acquried by another user.
 */
typedef struct {
    NestedQueue *const queue;
    int current_i;
    const int end_i;
} NestedQueueIterator;


/** Initialize a #NestedQueueIterator for iterating the read acquired region
 *
 * \param q the queue
 * \return A #NestedQueueIterator over the read acquired region of \p q
 *
 * \note You must ensure that this region is exclusively acquired and not
 * released while the iterator is used.
 */
NestedQueueIterator NestedQueueIterator_init_read(NestedQueue *q);


/** Initialize a #NestedQueueIterator for iterating the write acquired region
 *
 * \param q the queue
 * \return A #NestedQueueIterator over the write acquired region of \p q
 *
 * \note You must ensure that this region is exclusively acquired and not
 * committed while the iterator is used.
 */
NestedQueueIterator NestedQueueIterator_init_write(NestedQueue *q);


/** Return a pointer to the next slot in the iterator
 *
 * \param iterator the #NestedQueueIterator
 * \return a pointer to the next slot that is iterated over
 * \retval NULL if the end of the iterator is reached
 */
void *NestedQueueIterator_next(NestedQueueIterator *iterator);


#endif /* ifndef AINT_SAFE__NESTED_QUEUE_H */
