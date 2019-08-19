#ifndef _TARGET_H
#define _TARGET_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _libitm_jmpbuf {
	void* cfa;
	uint64_t rbx;
	uint64_t rbp;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
	uint64_t rip;
} libitm_jmpbuf;

uint32_t __attribute__((noreturn)) __attribute__((visibility("hidden")))
libitm_longjmp (libitm_jmpbuf* jmpbuf, uint32_t abort_flags);

#ifdef __cplusplus
}
#endif


#endif /* _TARGET_H */
