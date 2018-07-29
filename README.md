aint-safe - Lock-free async-interrupt-safe data structures
----------------------------------------------------------

aint-safe is a collection of lock-free data structures targeted for use in
interrupt-based (mostly embedded) single-threaded systems. It uses C11 atomics
only.


## Features
- Purely [C11 atomics](http://en.cppreference.com/w/c/atomic).
- Lock-free up to the C11 implementation of `stdatomic`
- No dynamic memory use - `malloc`.

## Note

The functions ensure that the _visible_ effect of any operation interrupting
(nesting within) any other operation is the same as the nested operation
occuring _just before_ or _just after_ the interrupted operation.

_You_ must take into account that a nested operation may change the data
structure state before or after one of the atomic operations.
