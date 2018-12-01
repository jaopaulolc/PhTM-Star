#ifndef _THREAD_H
#define _THREAD_H

#include <stdint.h>
#include "target.h"
#include "undolog.h"

#ifdef __cplusplus
extern "C" {
#endif

struct threadDescriptor_t {
	jmpbuf_t jmpbuf;
	void* stmTxDescriptor; 
	undolog_t undolog;
	uint32_t id;
	uint32_t depth;
	uint32_t codeProperties;
	bool aborted;

	void* operator new (size_t size);
	void operator delete (void* tx);
};

extern __thread threadDescriptor_t* __threadDescriptor;

void initThreadDescriptor(threadDescriptor_t* tx);

static inline threadDescriptor_t*
getThreadDescriptor() {
	return __threadDescriptor;
}

static inline void
setThreadDescriptor(threadDescriptor_t* td) {
	__threadDescriptor = td;
}

#ifdef __cplusplus
}
#endif

#endif /* _THREAD_H */
