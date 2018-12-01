#include <stdlib.h>
#include "api/api.hpp"
#include "stm/txthread.hpp"
#include "libitm.h"

void *_ITM_MALLOC ITM_PURE
_ITM_malloc (size_t size) {
	return stm::tx_alloc (size);
}

void *_ITM_MALLOC ITM_PURE
_ITM_calloc (size_t nmemb, size_t size) {
	void* ptr = stm::tx_alloc(nmemb*size);
	memset(ptr, 0, nmemb*size);
	return ptr;
}

void ITM_PURE
_ITM_free (void *addr) {
	stm::tx_free (addr);
}
