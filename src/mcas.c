/** \file membag.c
 *
 * Memory bag implementation with static storage
 */
/* Copyright 2018 Gaurav Juvekar */

#include "mcas.h"
#include <assert.h>
#include <stdbool.h>

/* For multi-word compare-and-swap, we first build up a linked list of
 * "operations to do" called a journal. We append the operation that we want to
 * do at the end of this list. Then, every caller will try to complete all the
 * operations in the journal list before attempting the operation at the end of
 * the list.
 * Essentially, every interrupt will first try and complete the work of the
 * operation that it interrupted before performing it's own operations.
 */

/** An operation status */
typedef enum {
    MCAS_STATUS_SUCCESS,
    MCAS_STATUS_FAILURE,
    MCAS_STATUS_UNDEFINED,
} McasStatus;

_Static_assert(atomic_is_lock_free((McasStatus *)NULL),
               "McasStatus enum should be lock-free.");

/* Type of operation */
typedef enum {
    MCAS_OPERATION_READ,
    MCAS_OPERATION_CAS,
} McasOperation;

_Static_assert(atomic_is_lock_free((McasOperation *)NULL),
               "McasOperation enum should be lock-free.");


struct McasJournal {
    /** Pointer to the next operation in the journla list (or NULL) */
    McasJournal *_Atomic operation_chain;
    /** Status of this operation */
    _Atomic McasStatus  status;
    /** The actual operation */
    const McasOperation operation;
    union {
        /* For Mcas_compare_exchange */
        struct {
            /** array of expected values */
            const int *const expected;
            /** array of desired values */
            const int *const desired;
            /** Internal flag to see if the data == expected for all values */
            /* We start swapping if data == expected, otherwise, data ==
             * expected is still being compared */
            atomic_bool swapping;
        };
        /* For Mcas_read */
        struct {
            /** Destination to store the values */
            int *const read_dest;
            atomic_flag *const read_flags;
        };
    };
};


static McasJournal *_Atomic *link_McasJournal(Mcas *       mcas,
                                              McasJournal *journal) {
    McasJournal *_Atomic *j    = &mcas->journal;
    McasJournal *         next = NULL;
    while (!atomic_compare_exchange_strong(j, &next, journal)) {
        j    = &next->operation_chain;
        next = NULL;
    }
    return j;
}


static void complete_mcas(Mcas *mcas, McasJournal *journal) {
    McasStatus status = atomic_load(&journal->status);
    if (status == MCAS_STATUS_UNDEFINED) {
        if (!atomic_load(&journal->swapping)) {
            /* Still comparing */
            for (size_t i = 0; i < mcas->n_elems; i++) {
                if (atomic_load(&mcas->data[i]) != journal->expected[i]) {
                    /* We need the strong version so that there aren't any
                     * spurious failures. If journal->status has changed, it
                     * could be a SUCCESS or a FAILURE. A success means that
                     * someone completed the MCAS or data != expected. In
                     * either cases, the MCAS has been completed and we just
                     * return. Otherwise, we set it to a FAILURE and return */
                    atomic_compare_exchange_strong(
                            &journal->status, &status, MCAS_STATUS_FAILURE);
                    return;
                }
            }
            /* data == expected, now to actually set desired => data */
            atomic_store(&journal->swapping, true);
        }
        /* Now, we set data to desired value (compare is successful) */
        for (size_t i = 0; i < mcas->n_elems; i++) {
            atomic_store(&mcas->data[i], journal->desired[i]);
        }
        atomic_store(&journal->status, MCAS_STATUS_SUCCESS);
    }
}


static void complete_read(Mcas *mcas, McasJournal *journal) {
    if (atomic_load(&journal->status) == MCAS_STATUS_UNDEFINED) {
        for (size_t i = 0; i < mcas->n_elems; i++) {
            int value = atomic_load(&mcas->data[i]);
            if (!atomic_flag_test_and_set(&journal->read_flags[i])) {
                /* No need for this write to dest to be atomic as the flag acts
                 * like a once-only mutex */
                journal->read_dest[i] = value;
            }
        }
        atomic_store(&journal->status, MCAS_STATUS_SUCCESS);
    }
}


static void complete_operation(Mcas *mcas, McasJournal *journal) {
    switch (journal->operation) {
    case MCAS_OPERATION_READ: complete_read(mcas, journal); break;
    case MCAS_OPERATION_CAS: complete_mcas(mcas, journal); break;
    }
}

static void execute_operation(Mcas *mcas, McasJournal *journal) {
    McasJournal *_Atomic *prev_node = link_McasJournal(mcas, journal);
    /* Traverse the journal chain and complete operations */
    for (McasJournal *j = atomic_load(&mcas->journal); j != NULL;
         j              = atomic_load(&j->operation_chain)) {
        complete_operation(mcas, j);
    }
    /* Must be NULL as this function preserves state with nesting. Every
     * appended journal entry is unlinked before it returns */
    assert(atomic_load(&journal->operation_chain) == NULL);
    /* unlink this journal that we added. We could equivalently just set the
     * previous journal's chain pointer to NULL (see previous assert) */
    atomic_store(prev_node, journal->operation_chain);
    assert(atomic_load(&journal->operation_chain) == NULL);
}


_Bool Mcas_compare_exchange(Mcas *     mcas,
                            const int *expected,
                            const int *desired) {
    McasJournal journal = {
            .operation_chain = NULL,
            .operation       = MCAS_OPERATION_CAS,
            .status          = MCAS_STATUS_UNDEFINED,
            .expected        = expected,
            .desired         = desired,
            .swapping        = false,
    };
    execute_operation(mcas, &journal);
    McasStatus status = atomic_load(&journal.status);
    if (status == MCAS_STATUS_SUCCESS) {
        return true;
    } else {
        return false;
    }
}


_Bool Mcas_read(Mcas *mcas, int *data) {
    /* one flag after reading one int. The call that acquires this flag will
     * write it to the destination. */
    atomic_flag read_flags[mcas->n_elems];

    for (size_t i = 0; i < mcas->n_elems; i++) {
        atomic_flag_clear(&read_flags[i]);
    }
    McasJournal journal = {
            .operation_chain = NULL,
            .operation       = MCAS_OPERATION_READ,
            .status          = MCAS_STATUS_UNDEFINED,
            .read_dest       = data,
            .read_flags      = read_flags,
    };

    execute_operation(mcas, &journal);
    assert(atomic_load(&journal.status) == MCAS_STATUS_SUCCESS);
    return true;
}
