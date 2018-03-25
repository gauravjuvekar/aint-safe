/** \file membag.c
 *
 * Memory bag implementation with static storage
 */
/* Copyright 2018 Gaurav Juvekar */

#include "membag.h"


void Membag_init(Membag *membag) {
    atomic_init(&membag->n_free, membag->n_elems);
    for (size_t i = 0; i < membag->n_elems; i++) {
        atomic_flag_clear(&membag->alloc_status[i]);
    }
}


void *Membag_acquire(Membag *membag) {
    int acquired = atomic_fetch_sub(&membag->n_free, 1);
    if (!(acquired > 0)) {
        /* Restore the acquire as there is no free slot available */
        atomic_fetch_add(&membag->n_free, 1);
        return NULL;
    } else {
        /* We have reserved a free slot somewhere in the membag. Now to
         * actually find and acquire it */
        size_t i = 0;
        while (atomic_flag_test_and_set(&membag->alloc_status[i])) {
            i = (i + 1) % membag->n_elems;
        }
        return (char *)membag->data + (membag->elem_size * i);
    }
}


void Membag_release(Membag *membag, const void *element) {
    if (element == NULL) return;
    const size_t idx =
            ((char *)element - (char *)membag->data) / membag->elem_size;
    /* Simple enough, though note that a "double release" will wreak havoc with
     * acquire as n_free is incremented without actually releasing a slot. This
     * may cause acquire() to be stuck in an infinite loop as it will search
     * for a slot that it acquried, but which doesn't actually exist.
     *
     * This can be fixed by checking the value in the status array before
     * blindly clearing it, though reading the value of a atomic_flag is not
     * possible without modifying it */
    atomic_flag_clear(&membag->alloc_status[idx]);
    atomic_fetch_add(&membag->n_free, 1);
}
