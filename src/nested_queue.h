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

#if !defined(__DOXYGEN__AINT_SAFE__)
_Static_assert(
        ATOMIC_POINTER_LOCK_FREE,
        "Your stdlib implementation does not have lock-free pointer atomics");
#endif


/** \brief Internal data structure of the nested MPMC queue
 *
 * This must be initialized with #NESTED_QUEUE_STATIC_INIT at declaration
 */
typedef struct NestedQueue {
    /** Pointer to next slot in data that can be acquired for writing */
    void *_Atomic write_allocated;
    /** Pointer to first slot in data that is being written to */
    void *_Atomic write_committed;
    /** Pointer to next slot in data that can be acquired for reading */
    void *_Atomic read_acquired;
    /** Pointer to oldest slot in data that is being read */
    void *_Atomic read_released;
    /** Data to allocate slots from */
    void *const data;
    /** Number of elements in #data */
    const size_t n_elems;
    /** Size of a slot in #data */
    const size_t elem_size;
} NestedQueue;


/** \brief Statically initialize a #NestedQueue
 *
 * Use this macro to initialize a #NestedQueue at declaration.
 *
 * \param p_elem_size    size of one element of \p data
 * \param p_n_elems      number of elements in \p data
 * \param p_data_array   data array to allocate from
 *
 * \return A #NestedQueue instance with \c const members initialized
 */
#define NESTED_QUEUE_STATIC_INIT(p_elem_size, p_n_elems, p_data_array)        \
    {                                                                         \
        .data = p_data_array, .n_elems = p_n_elems, .elem_size = p_elem_size, \
        .write_allocated = p_data_array, .write_committed = p_data_array,     \
        .read_acquired = p_data_array, .read_released = p_data_array          \
    }


/** \brief Acquire an available slot from the queue for writing
 *
 * \param q #NestedQueue to acquire the slot from
 *
 * \return Pointer to an available slot in \p q
 * \retval NULL if no slot is available in \p q
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
 * \pre #NestedQueue_write_commit() must be called for any intermediate slots
 * acquired after the corresponding #NestedQueue_write_acquire() call
 */
void NestedQueue_write_commit(NestedQueue *q, const void *slot);


/** \brief Acquire a slot from the queue for reading
 *
 * \param q #NestedQueue to acquire the slot from
 *
 * \return Pointer to an available slot in \p q for reading
 * \retval NULL if no slot is available in \p q for reading
 *
 * \pre \p q must be initialized with #NESTED_QUEUE_STATIC_INIT
 * \post #NestedQueue_read_release() must be called after using the slot
 */
void *NestedQueue_read_acquire(NestedQueue *q);


/** \brief Release a slot acquired for reading and release its memory
 *
 * \param q    #NestedQueue to from which \p slot was acquired
 * \param slot slot acquired by #NestedQueue_read_acquire()
 *
 * \pre #NestedQueue_read_release() must be called for any intermediate slots
 * acquired after the corresponding #NestedQueue_read_acquire() call
 */
void NestedQueue_read_release(NestedQueue *q, const void *slot);


#endif /* ifndef AINT_SAFE__NESTED_QUEUE_H */
