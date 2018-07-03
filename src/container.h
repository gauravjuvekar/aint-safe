/** \file container.h
 *
 * Finding the container for generic datastructures, borrowed from the Linux
 * kernel.
 *
 */
#ifndef CONTAINER_H
#define CONTAINER_H 1

/** \brief Get the container struct from member address and type
 * See https://stackoverflow.com/questions/15832301/understanding-container-of-macro-in-the-linux-kernel
 * \param ptr    address of member
 * \param type   type of the parent struct
 * \param member member name of \p ptr within \p type
 *
 * \return address of \p type structure containing \p
 */
#define CONTAINER_OF(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

#endif /* ifndef CONTAINER_H */
