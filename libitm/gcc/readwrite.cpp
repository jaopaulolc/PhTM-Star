#include "libitm.h"
#include "thread.h"
#include "config/common.h"
#include "api/api.hpp"
#include "stm/txthread.hpp"


/*
		threadDescriptor_t* tx = getThreadDescriptor(); \
		void *top = mask_stack_top(tx); \
		void *bot = mask_stack_bottom(); \
		void *ptr = (void*)addr; \
		if (ptr < top && ptr >= bot) return *addr; \
		
		threadDescriptor_t* tx = getThreadDescriptor(); \
		void *top = mask_stack_top(tx); \
		void *bot = mask_stack_bottom(); \
		void *ptr = (void*)addr; \
		if (ptr < top && ptr >= bot) { \
			*addr = value; \
		} else { \
			stm::stm_write(addr, value, (stm::TxThread*)stm::Self); \
		}	\
	*/

#define ITM_READ(TYPE, LSMOD, EXT) \
	ITM_REGPARM TYPE _ITM_##LSMOD##EXT (const TYPE *addr) { \
		return stm::stm_read(addr, (stm::TxThread*)stm::Self); \
	}

#define ITM_WRITE(TYPE, LSMOD, EXT) \
	ITM_REGPARM void _ITM_##LSMOD##EXT (TYPE *addr, TYPE value) { \
		stm::stm_write(addr, value, (stm::TxThread*)stm::Self); \
	}

// ABI Section 5.12
#define ITM_BARRIERS(TYPE, EXT) \
  ITM_READ(TYPE, R, EXT) \
  ITM_READ(TYPE, RaR, EXT) \
  ITM_READ(TYPE, RaW, EXT) \
  ITM_READ(TYPE, RfW, EXT) \
  ITM_WRITE(TYPE, W, EXT) \
  ITM_WRITE(TYPE, WaR, EXT) \
  ITM_WRITE(TYPE, WaW, EXT) \

ITM_BARRIERS (uint8_t, U1)
ITM_BARRIERS (uint16_t, U2)
ITM_BARRIERS (uint32_t, U4)
ITM_BARRIERS (uint64_t, U8)
ITM_BARRIERS (float, F)
ITM_BARRIERS (double, D)
ITM_BARRIERS (long double, E)
//ITM_BARRIERS (float _Complex, CF)
//ITM_BARRIERS (double _Complex, CD)
//ITM_BARRIERS (long double _Complex, CE)
#if defined(__i386__) || defined(__x86_64__)
# ifdef __MMX__
	ITM_BARRIERS (__m64, M64)
# endif
# ifdef __SSE__
	ITM_BARRIERS (__m128, M128)
# endif
# ifdef __AVX__
	ITM_BARRIERS (__m256, M256)
# endif
#endif													/* i386 */
#undef ITM_BARRIERS
