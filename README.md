aint-safe - Lock-free async-interrupt-safe data structures
----------------------------------------------------------

aint-safe is a collection of lock-free data structures targeted for use in
interrupt-based (mostly embedded) single-threaded systems. It uses C11 atomics
only.


## Features
- Purely [C11 atomics](http://en.cppreference.com/w/c/atomic).
- Lock-free up to the C11 implementation of `stdatomic`
- No dynamic memory use - `malloc`.
