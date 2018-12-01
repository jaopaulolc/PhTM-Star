#include <stdio.h>
#include <memory.h>
#include "config/common.h"
#include "libitm.h"
#include "thread.h"
#include "undolog.h"

undolog_t::undolog_t() {

#ifdef DEBUG
	fprintf(stderr, "debug: undolog_t constructor called\n");
#endif
}
undolog_t::~undolog_t() {
#ifdef DEBUG
	fprintf(stderr, "debug: undolog_t destructor called\n");
#endif
}

void
undolog_t::log(const void* addr, size_t len) {

	size_t payload = (len + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);
	uintptr_t* undo = undolog.push(payload + 2);
	//fprintf(stderr, "log: sizeof(uintptr_t) =  %ld\n", sizeof(uintptr_t));
	//fprintf(stderr, "log: addr = %lx    len = %ld\n", (uint64_t)addr, len);
	// save log as a stack for ease rollback in reverse order
	memcpy(undo, addr, len);
	undo[payload] = (uintptr_t) len;
	undo[payload+1] = (uintptr_t) addr;
}

void NO_INLINE
undolog_t::rollback () {

#ifdef DEBUG
	fprintf(stderr, "debug: undolog_t rollback called!\n");
#endif
	threadDescriptor_t* tx = getThreadDescriptor();
	void* top = mask_stack_top(tx);
  void* bot = mask_stack_bottom();
	size_t i, size = undolog.getSize();
	if (size > 0) {
		for (i = size; i-- > 0; ) {
			void* addr = (void*)undolog[i--];
			size_t len = undolog[i];
			//fprintf(stderr, "rollback: addr = %lx    len = %ld\n", (uint64_t)addr, len);
			size_t payload = (len + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);
			i -= payload;
			if (likely(addr > top || (uint8_t*)addr + len <= bot))
				memcpy(addr, &undolog[i], len);
		}
		undolog.setSize(0);
	}
}

void
undolog_t::commit () {
	undolog.setSize(0);
}

void ITM_REGPARM
_ITM_LB (const void* addr, size_t len) {
	threadDescriptor_t* tx = getThreadDescriptor(); 
	fprintf(stderr, "debug: undolog_t rollback called!\n");
	void *top = mask_stack_top(tx);
	void *bot = mask_stack_bottom();
	void *ptr = (void*)addr;
	if (ptr < top && ptr >= bot) return;
	tx->undolog.log(addr, len);
}


#define ITM_LOG(TYPE, EXT) \
	void ITM_REGPARM _ITM_L##EXT (const TYPE* addr) { \
		threadDescriptor_t* tx = getThreadDescriptor(); \
		void *top = mask_stack_top(tx); \
		void *bot = mask_stack_bottom(); \
		void *ptr = (void*)addr; \
		if (ptr <= top && ptr > bot) return; \
		tx->undolog.log(addr, sizeof(TYPE)); \
	}

ITM_LOG (uint8_t, U1)
ITM_LOG (uint16_t, U2)
ITM_LOG (uint32_t, U4)
ITM_LOG (uint64_t, U8)
ITM_LOG (float, F)
ITM_LOG (double, D)
ITM_LOG (long double, E)
//ITM_LOG (float _Complex, CF)
//ITM_LOG (double _Complex, CD)
//ITM_LOG (long double _Complex, CE)

#if defined(__i386__) || defined(__x86_64__)
# ifdef __MMX__
  ITM_LOG (__m64, M64)
# endif
# ifdef __SSE__
	ITM_LOG (__m128, M128)
# endif
# ifdef __AVX__
	ITM_LOG (__m256, M256)
# endif
#endif													/* i386 */
#undef ITM_LOG
