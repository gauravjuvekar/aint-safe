*Interrupt
        - safe singly linkedl list * /
/* Copyright 2018 Gaurav Juvekar */

#include "slist.h"
#include <assert.h>
#include <stdbool.h>


static SlistNode *Slist_next_stable_until(SlistNode *node, SlistNode *limit);

static SlistNode *Slist_next_stable_until(SlistNode *node, SlistNode *limit) {
    SlistNode *next = atomic_load(&node->next);
    while (next != NULL && next != limit) {
        if (atomic_load(&next->deleting)) { /* Skip nodes being deleted */
            next = atomic_load(&next->next);
        } else {
            break;
        }
    }
    return next;
}


SlistNode *Slist_next(SlistNode *node) {
    return Slist_next_stable_until(node, NULL);
}

SlistNode *Slist_append(SlistNode *node, SlistNode *new) {
    if (atomic_load(&node->deleting)) {
        /* Don't modify a 'deleting' node. This is mostly a user error since a
         * node being currently deleted should not usually be used and passed
         * to any functions (though it is not illegal to do so) */
        return NULL;
    }
    atomic_store(&new->deleting, false);
    SlistNode *next = atomic_load(&node->next);
    do {
        /* If `node` is deleted now (till the compare_exchange), node->next
         * will be set to NULL and the compare_exchange will fail. The append
         * will succeed on the next iteration appending to the deleted node.
         * This is _visibly_ the same as the delete occuring just before the
         * ongoing append operation. */
        atomic_store(&new->next, next);
    } while (!atomic_compare_exchange_strong(&node->next, &next, new));
    return new;
}


SlistNode *Slist_delete_after(SlistNode *node, SlistNode *to_delete) {
    if (atomic_load(&node->deleting)) {
        /* Don't modify a 'deleting' node. This is mostly a user error since a
         * node being currently deleted should not usually be used and passed
         * to any functions (though it is not illegal to do so) */
        return NULL;
    }

    atomic_store(&to_delete->deleting, true);
    /* Find the node just before to_delete */
    SlistNode *prev = node;
    while (prev != NULL && prev != to_delete) {
        if (atomic_load(&prev->next) == to_delete) {
            SlistNode *next_of_to_delete = atomic_load(to_delete->next);
            if (!atomic_compare_exchange_strong(
                        &prev->next, to_delete, next_of_to_delete)) {
                continue; /* Someone inserted something in between prev and
                             to_delete */
            }
            if (!atomic_compare_exchange_strong(
                        &to_delete->next, next_of_to_delete, NULL)) {
                /* Someone appended to the 'deleting' node.
                 * This should never happen. ('deleting' is a lock for the
                 * current node) */
                assert(false);
            }
            atomic_store(&to_delete->deleting, false);
            return ;
        }
        prev = Slist_next_stable_until(prev, to_delete);
    }
    /* to_delete was not found in the list */
    assert(false);
}
