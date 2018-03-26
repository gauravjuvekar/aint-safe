/** \file double_buffer.c
 *
 * Nested multiple-producer multiple-consumer double buffer to store the most
 * recent value with static storage.
 */
/* Copyright 2018 Gaurav Juvekar */
#include "double_buffer.h"


void *DoubleBuffer_write_acquire(DoubleBuffer *db) {
    if (atomic_flag_test_and_set(&db->write_mutex)) {
        /* Another writer is writing */
        return NULL;
    } else {
        /* Now we are the exclusive writer */
        /* Write the current slot being read into the next slot to read, so
         * that that no reader will touch the remainig slot.
         * This must be done in a loop since a reader can change the current
         * slot begin read between the atomic_load and the atomic_exchange */
        void *last_selected;
        do {
            last_selected = atomic_load(&db->selected_read);
        } while (last_selected
                 != atomic_exchange(&db->next_read, last_selected));
        /* Now both current read and next to read point to the same slot. We
         * will now actually acquire the other slot for writing. Readers can
         * now keep reading from the last_selected slot. */
        void *acquired = last_selected == db->data ?
                                 (char *)db->data + db->elem_size :
                                 db->data;
        return acquired;
    }
}


void DoubleBuffer_write_commit(DoubleBuffer *db, void *slot) {
    atomic_store(&db->next_read, slot);
    atomic_flag_clear(&db->write_mutex);
}


void *DoubleBuffer_read_acquire(DoubleBuffer *db) {
    if (0 == atomic_fetch_add(&db->n_readers, 1)) {
        /* we are the first reader */
        /* We check if there is a new slot to read from, and set it to
         * the current slot being read. This must be done in a loop, as a
         * writer may change the next slot to read from between the atomic_load
         * and the atomic_exchange */
        void *last_next_read;
        do {
            last_next_read = atomic_load(&db->next_read);
        } while (last_next_read
                 != atomic_exchange(&db->selected_read, last_next_read));
    }
    return atomic_load(&db->selected_read);
}


void DoubleBuffer_read_release(DoubleBuffer *db, const void *slot) {
    /* We don't really need the actual slot that was being read in this
     * implementation */
    (void)slot;
    atomic_fetch_sub(&db->n_readers, 1);
}
