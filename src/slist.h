/** \file slist.h
 *
 * Interrupt-safe singly linked list
 *
 * Usage:
 * \code{.c}
 * \endcode
 */
/* Copyright 2019 Gaurav Juvekar */

#ifndef AINT_SAFE__SLIST_H
#define AINT_SAFE__SLIST_H 1

#include <stdatomic.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


#if !defined(__DOXYGEN__AINT_SAFE__)
_Static_assert(
        ATOMIC_POINTER_LOCK_FREE,
        "Your stdlib implementation does not have lock-free pointer atomics");
#endif

struct SlistNode {
    _Atomic bool deleting;
    struct SlistNode * _Atomic next;
};
typedef struct SlistNode SlistNode;
typedef SlistNode * _Atomic Slist;

SlistNode *Slist_next(SlistNode *node);
SlistNode *Slist_append(SlistNode *node, SlistNode *new);
SlistNode *Slist_delete_after(SlistNode *node, SlistNode *to_delete);


#endif /* ifndef AINT_SAFE__SLIST_H */
