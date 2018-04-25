/** \file double_buffer.h
 *
 * Nested multiple-producer multiple-consumer double buffer to store the most
 * recent value with static storage.
 *
 * Usage:
 * \code{.c}
 * typedef struct {...} MyStruct;
 *
 * // Must be initialized to some sane value as this can be returned to a
 * // reader initially.
 * static MyStruct array[2];
 *
 * DoubleBuffer global_latest_value = DOUBLE_BUFFER_STATIC_INIT(
 *         sizeof(MyStruct), array);
 *
 * void handler_writer(void) {
 *     // some interrupt, eg systick, or value from some sensor
 *     MyStruct *latest = DoubleBuffer_write_acquire(&global_latest_value);
 *     if (latest != NULL) {
 *         // write value into *latest
 *     }
 *     DoubleBuffer_write_commit(&global_latest_value, latest);
 * }
 *
 * void handler2(void) {
 *     // periodically check the latest value
 *     MyStruct *latest = DoubleBuffer_read_acquire(&global_latest_value);
 *     ... // Do something with *latest
 *     DoubleBuffer_read_release(&global_latest_value, latest);
 * }
 *
 * void handler_n(void) {
 *     // Can be just like handler2
 *     handler2();
 * }
 *
 * // handler_writer, handler2, ... handler_n can have any priorities. Any of
 * // them can interrupt any other handler.
 * \endcode
 *
 * \note acquire() and commit()/release() must happen in a nested order. The
 * best way to ensure this is that each function calls acqurire() only once and
 * calls the corresponding commit()/release() before it returns.
 */
/* Copyright 2018 Gaurav Juvekar */

#ifndef AINT_SAFE__DOUBLE_BUFFER_H
#define AINT_SAFE__DOUBLE_BUFFER_H 1
#include <stdatomic.h>
#include <stddef.h>

#if !defined(__DOXYGEN__AINT_SAFE__)
_Static_assert(
        ATOMIC_POINTER_LOCK_FREE,
        "Your stdlib implementation does not have lock-free pointer atomics");
_Static_assert( ATOMIC_INT_LOCK_FREE,
        "Your stdlib implementation does not have lock-free atomic int");
#endif


/** \brief Internal data structure of the nested MPMC double buffer
 *
 * This must be initialized with #DOUBLE_BUFFER_STATIC_INIT at declaration.
 */
typedef struct {
    /** Pointer to current slot in #data being read */
    void *_Atomic selected_read;
    /** Pointer to next slot in #data that can be read from */
    void *_Atomic next_read;
    /** Data array (of length 2) forming the double buffer */
    void *const data;
    /** Size of a slot in #data */
    const size_t elem_size;
    /** Number of readers currently reading */
    _Atomic int n_readers;
    /** Write mutex that allows only one writer at a time */
    atomic_flag write_mutex;
} DoubleBuffer;


/** \brief Statically initialize a #DoubleBuffer
 *
 * \param p_elem_size  size of one element of \p p_data_array
 * \param p_data_array data array (of length 2) to use as the double buffer
 *
 * \return A #DoubleBuffer static initializer
 */
#define DOUBLE_BUFFER_STATIC_INIT(p_elem_size, p_data_array)      \
    {                                                             \
        .data = p_data_array, .elem_size = p_elem_size,           \
        .selected_read = p_data_array, .next_read = p_data_array, \
        .n_readers = 0, .write_mutex = ATOMIC_FLAG_INIT           \
    }


/** \brief Acquire a slot for writing
 *
 * \param db #DoubleBuffer to acquire the slot from
 *
 * \return Pointer to an available slot in \p db->data
 * \retval NULL if another writer has acquired the slot first
 *
 * \pre \p db must be initialized with #DOUBLE_BUFFER_STATIC_INIT
 * \post #DoubleBuffer_write_commit() must be called after writing to the
 * returned slot.
 */
void *DoubleBuffer_write_acquire(DoubleBuffer *db);


/** \brief Commit a slot previously acquired for writing
 *
 * \param db   #DoubleBuffer from which \p slot was acquired
 * \param slot slot acquired by #DoubleBuffer_write_acquire() or NULL
 *
 * \pre #DoubleBuffer_write_commit() must be called for any intermediate slots
 * acquired after the corresponding #DoubleBuffer_write_acquire() call
 */
void DoubleBuffer_write_commit(DoubleBuffer *db, void *slot);


/** \brief Acquire a slot for reading
 *
 * \param db #DoubleBuffer to acquire the slot from
 *
 * \return Pointer to an available slot in \p db->data for reading
 *
 * \pre \p db must be initialized with #DOUBLE_BUFFER_STATIC_INIT
 * \post #DoubleBuffer_read_release() must be called after using the slot
 */
const void *DoubleBuffer_read_acquire(DoubleBuffer *db);


/** \brief Release a slot previously acquired for reading
 *
 * \param db   #DoubleBuffer from which \p slot was acquired
 * \param slot slot acquired by #DoubleBuffer_read_acquire() or NULL
 *
 * \pre #DoubleBuffer_read_release() must be called for any intermediate slots
 * acquired after the corresponding #DoubleBuffer_read_acquire() call
 */
void DoubleBuffer_read_release(DoubleBuffer *db, const void *slot);


#endif /* ifndef AINT_SAFE__DOUBLE_BUFFER_H */
