/** \file mcas.h
 *
 * Interrupt-safe Multiword Compare-And-Swap implementation
 *
 * Usage:
 * \code{.c}
 * \endcode
 */
/* Copyright 2018 Gaurav Juvekar */

#ifndef AINT_SAFE__MCAS_H
#define AINT_SAFE__MCAS_H 1

#include <stdatomic.h>
#include <stdint.h>
#include <stddef.h>


typedef intptr_t mcas_base_t;

#if !defined(__DOXYGEN__AINT_SAFE__)
_Static_assert(
        atomic_is_lock_free((mcas_base_t *)NULL),
        "Your stdlib implementation does not have lock-free atomics for the base unit");
_Static_assert(
        ATOMIC_POINTER_LOCK_FREE,
        "Your stdlib implementation does not have lock-free pointer atomics");
#endif


/* Forward declaration */
typedef struct McasJournal McasJournal;

/** \brief Internal data structure of the MCAS
 *
 * This must be initialized with #MCAS_STATIC_INIT at declaration.
 */
typedef struct {
    /** MCAS words */
    _Atomic mcas_base_t *const data;
    /** Number of elements in #data */
    const size_t n_elems;
    /** Internal "intent-log" journal to use while operating on the data */
    McasJournal *_Atomic journal;
} Mcas;


/** \brief Statically initialize a Mcas
 *
 * Use this macro to initialize a #Mcas at declaration.
 *
 * \param p_n_elems    number of elements in \p data
 * \param p_data_array data array of <tt>_Atomic  mcas_base_t</tt> to allocate from
 *
 * \return A #Mcas static initializer
 */
#define MCAS_STATIC_INIT(p_n_elems, p_data_array) \
    { .data = p_data_array, .n_elems = p_n_elems, .journal = NULL }


/** \brief Read an MCAS array
 * \param mcas structure to read from
 * \param data array of length \p mcas->n_elems to read into
 *
 * \retval true  if the read is successful
 * \retval false if the read fails
 */
_Bool Mcas_read(Mcas *mcas, mcas_base_t *data);


/** \brief Atomicaly compare and swap values of the MCAS
 *
 * Atomically compares the values pointed to by \p mcas->data with \p expected,
 * and if those are equal, replaces \p mcas->data with values from \p desired.
 *
 * \param mcas     to perform the CAS on
 * \param expected values before the CAS (array of \p mcas->n_elems
 * mcas_base_t) \param desired  values after the CAS (array of \p mcas->n_elems
 * mcas_base_t)
 *
 * \retval true  if the MCAS opearation succeeded
 * \retval false otherwise
 *
 * \note This function does \b NOT replace \p expected with the current values
 * if \p mcas->data and \p expected are not equal.
 */
_Bool Mcas_compare_exchange(Mcas *             mcas,
                            const mcas_base_t *expected,
                            const mcas_base_t *desired);

#endif /* ifndef AINT_SAFE__MCAS_H */
