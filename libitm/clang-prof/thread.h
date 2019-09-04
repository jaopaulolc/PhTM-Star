#ifndef _THREAD_H
#define _THREAD_H

#include <stdint.h>
#include "undolog.h"

#include <unordered_map>
#include <unordered_set>

#ifdef __cplusplus
extern "C" {
#endif

struct threadDescriptor_t {
	libitm_jmpbuf *jmpbuf;
	void* stmTxDescriptor; 
	undolog_t undolog;
	uint32_t id;
	uint32_t depth;
	uint32_t codeProperties;
	bool aborted;
  uint64_t numberOfReads;
  uint64_t numberOfWrites;
  uint64_t numberOfStackReads;
  uint64_t numberOfStackWrites;
  uint64_t numberOfHeapReads;
  uint64_t numberOfHeapWrites;
  uint64_t numberOfRequiredReads;
  uint64_t numberOfRequiredWrites;
  std::unordered_set<void*> heapLocalPointers;
  std::unordered_map<uint64_t, uint64_t>  mapStackReadsPCtoCounter;
  std::unordered_map<uint64_t, uint64_t>  mapStackWritesPCtoCounter;
  std::unordered_map<uint64_t, uint64_t>  mapGlobalHeapReadsPCtoCounter;
  std::unordered_map<uint64_t, uint64_t>  mapGlobalHeapWritesPCtoCounter;
  std::unordered_map<uint64_t, uint64_t>  mapLocalHeapReadsPCtoCounter;
  std::unordered_map<uint64_t, uint64_t>  mapLocalHeapWritesPCtoCounter;
  // "required" counters
  std::unordered_map<uint64_t, uint64_t>  mapReqStackReadsPCtoCounter;
  std::unordered_map<uint64_t, uint64_t>  mapReqStackWritesPCtoCounter;
  std::unordered_map<uint64_t, uint64_t>  mapReqGlobalHeapReadsPCtoCounter;
  std::unordered_map<uint64_t, uint64_t>  mapReqGlobalHeapWritesPCtoCounter;
  std::unordered_map<uint64_t, uint64_t>  mapReqLocalHeapReadsPCtoCounter;
  std::unordered_map<uint64_t, uint64_t>  mapReqLocalHeapWritesPCtoCounter;

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
