#ifndef _COMMON_H
#define _COMMON_H

#include "thread.h"

static inline void*
mask_stack_top(threadDescriptor_t *tx) {
	return tx->jmpbuf.cfa;
}

//extern void* mask_stack_bottom(threadDescriptor_t *tx UNUSED) __attribute__((noinline));
extern void* mask_stack_bottom() __attribute__((noinline));

#endif /* _COMMON_H */
