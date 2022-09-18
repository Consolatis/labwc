/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_MEM_H
#define __LABWC_MEM_H

#include <stdlib.h>

/*
 * As defined in busybox, weston, etc.
 * Allocates zero-filled memory; calls exit() on error.
 * Returns NULL only if (size == 0).
 */
void *xzalloc(size_t size);

/* Type-safe macros in the style of C++ new/new[] */
#define znew(type)       ((type *)xzalloc(sizeof(type)))
#define znew_n(type, n)  ((type *)xzalloc((n) * sizeof(type)))

/*
 * Variant of znew() that infers type from the passed pointer.
 * Preferred over plain znew() in pointer declarations since it
 * does not require repeating the type name.
 *
 * Example:
 *   struct wlr_box *box = znew_for(box);
 *
 * Note:
 *   In C23 "typeof" will be standard. Until then, __typeof__ works
 *   with both GCC and clang (do we care about other compilers?)
 */
#define znew_for(ptr)  ((__typeof__(&ptr))xzalloc(sizeof(ptr)))

/*
 * As defined in FreeBSD.
 * Like realloc(), but calls exit() on error.
 * Returns NULL only if (size == 0).
 * Does NOT zero-fill memory.
 */
void *xrealloc(void *ptr, size_t size);

/* malloc() is a specific case of realloc() */
#define xmalloc(size) xrealloc(NULL, (size))

/*
 * As defined in FreeBSD.
 * Allocates a copy of <str>; calls exit() on error.
 * Requires (str != NULL) and never returns NULL.
 */
char *xstrdup(const char *str);

/*
 * Frees memory pointed to by <ptr> and sets <ptr> to NULL.
 * Does nothing if <ptr> is already NULL.
 */
#define zfree(ptr) do { \
        free(ptr); (ptr) = NULL; \
} while (0)

#endif /* __LABWC_MEM_H */
