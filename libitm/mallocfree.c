#include <libitm.h>


#include <stm/txthread.hpp>

void *
__attribute__((__malloc__)) ITM_PURE
_ITM_malloc (size_t sz) {

	return stm::Self->allocator.txAlloc(sz);
}

/*
void *
_ITM_calloc (size_t nm, size_t sz) {

  void *r = calloc (nm, sz);
  if (r)
    gtm_thr()->record_allocation (r, free);
  return r;
}*/

void
ITM_PURE
_ITM_free (void *ptr) {

	stm::Self->allocator.txFree(ptr);
}


