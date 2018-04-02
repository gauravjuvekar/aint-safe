/** \file membag.h
 *
 * Memory bag implementation with static storage
 *
 * Usage:
 * \code{.c}
 * static struct MyStruct pool_array[10];
 * static membag_alloc_status_t my_membag_status[MEMBAG_ALLOC_STATUS_LEN(10)];
 * static Membag struct_pool = MEMBAG_STATIC_INIT(
 *     sizeof(struct MyStruct), 10, my_membag_status, pool_array);
 * ...
 *
 * main() {
 *     Membag_init(&struct_pool);
 *
 *     struct MyStruct *elem = Membag_acquire(&struct_pool);
 *     if (elem != NULL) {
 *         // Do something with *elem
 *         ...
 *     }
 *     ...
 *     Membag_release(&struct_pool, elem);
 * }
 * \endcode
 */
/* Copyright 2018 Gaurav Juvekar */

#ifndef AINT_SAFE__MEMBAG_H
#define AINT_SAFE__MEMBAG_H 1
#include <stdatomic.h>
#include <stddef.h>

#if !defined(__DOXYGEN__AINT_SAFE__)
_Static_assert(
        ATOMIC_INT_LOCK_FREE,
        "Your stdlib implementation does not have lock-free atomics for int");
#endif


/** \brief Array to store internal status of the membag
 *
 * Use #MEMBAG_ALLOC_STATUS_LEN to declare this array for a corresponding data
 * array with N_ELEMENTS.
 *
 * Example usage:
 * \code{.c}
 * membag_alloc_status_t my_membag1[MEMBAG_ALLOC_STATUS_LEN(10)];
 * static membag_alloc_status_t my_membag2[MEMBAG_ALLOC_STATUS_LEN(10)];
 * \endcode
 */
typedef atomic_flag membag_alloc_status_t;


/** \brief Calculate size of alloc_status array required for \p N_ELEMENTS data
 */
#define MEMBAG_ALLOC_STATUS_LEN(N_ELEMENTS) (N_ELEMENTS)


/** \brief Internal data structure of the membag
 *
 * This must be initialized with #MEMBAG_STATIC_INIT at declaration AND
 * #Membag_init at runtime.
 */
typedef struct Membag {
    /** Status array marking allocated slots */
    membag_alloc_status_t *const alloc_status;
    /** Data to allocate slots from */
    void *const data;
    /** Number of elements in #data */
    const size_t n_elems;
    /** Size of a slot in #data */
    const size_t elem_size;
    /** Number of slots currently free */
    _Atomic int n_free;
} Membag;


/** \brief Statically initialize a Membag
 *
 * Use this macro to initialize a #Membag at declaration.
 *
 * \param p_elem_size    size of one element of \p data
 * \param p_n_elems      number of elements in \p data
 * \param p_status_array #membag_alloc_status_t array of length
 *     \c #MEMBAG_ALLOC_STATUS_LEN(\p p_n_elems)
 * \param p_data_array   data array to allocate from
 *
 * \return A #Membag static initializer
 */
#define MEMBAG_STATIC_INIT(                                   \
        p_elem_size, p_n_elems, p_status_array, p_data_array) \
    {                                                         \
        .alloc_status = p_status_array, .data = p_data_array, \
        .n_elems = p_n_elems, .elem_size = p_elem_size        \
    }


/** \brief Initialize a #Membag instance at runtime
 *
 * \param membag #Membag to initialize
 *
 * \pre \p membag must be initialized with #MEMBAG_STATIC_INIT first
 */
void Membag_init(Membag *membag);


/** \brief Acquire an available slot from the membag
 *
 * \param membag #Membag to acquire the slot from
 *
 * \return Pointer to an available slot in \p membag->data
 * \retval NULL if no slot is available in \p membag->data
 *
 * \pre \p membag must be initialized with #Membag_init
 */
void *Membag_acquire(Membag *membag);


/** \brief Release an acquired slot
 *
 * \param membag #Membag that the slot belongs to
 * \param slot   pointer to a slot previously acquired by #Membag_acquire or
 *     \c NULL
 *
 *  \warning A "double release" / "double free" \b WILL wreak havoc with the
 *  entire data structure. Subsequent #Membag_acquire() calls may get stuck in
 *  infinite loops.
 */
void Membag_release(Membag *membag, const void *slot);


#endif /* ifndef AINT_SAFE__MEMBAG_H */
