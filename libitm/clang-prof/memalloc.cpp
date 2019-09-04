#include <stdlib.h>
#include <string.h>
#include "libitm.h"
#include "thread.h"

void *_ITM_MALLOC ITM_PURE
_ITM_malloc (size_t size) {
  /* FIXME -- if a transaction aborts, memory leaks! */
	void* ptr = malloc (size);
  threadDescriptor_t* tx = getThreadDescriptor();
  tx->heapLocalPointers.insert(ptr);
  return ptr;
}

void *_ITM_MALLOC ITM_PURE
_ITM_calloc (size_t nmemb, size_t size) {
  /* FIXME -- if a transaction aborts, memory leaks! */
	void* ptr = malloc(nmemb*size);
	memset(ptr, 0, nmemb*size);
  threadDescriptor_t* tx = getThreadDescriptor();
  tx->heapLocalPointers.insert(ptr);
	return ptr;
}

void ITM_PURE
_ITM_free (void *addr) {
	free (addr);
}
