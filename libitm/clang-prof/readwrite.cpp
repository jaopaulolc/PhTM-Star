#include "libitm.h"
#include "thread.h"
#include "config/common.h"

static void* last_addr_used = NULL;

#define PROFILING_CODE(GL_COUNTER,STACK_COUNTER, HEAP_COUNTER, \
      STACK_MAP, GL_HEAP_MAP, L_HEAP_MAP, REQ_STACK_MAP,\
      REQ_GL_HEAP_MAP, REQ_L_HEAP_MAP) \
    void* ret_addr = __builtin_return_address(0); \
    uint64_t ret_pc = (uint64_t)__builtin_extract_return_addr(ret_addr); \
    threadDescriptor_t* tx = getThreadDescriptor(); \
    GL_COUNTER++; \
    void *top = mask_stack_top(tx); \
    void *bot = mask_stack_bottom(); \
    void *ptr = (void*)addr; \
    if (last_addr_used == (void*)addr) { \
      last_addr_used = NULL; \
      if (ptr < top && ptr >= bot) { \
        REQ_STACK_MAP[ret_pc]++; \
        STACK_COUNTER++; \
      } else if (tx->heapLocalPointers.count(ptr) > 0) { \
        REQ_L_HEAP_MAP[ret_pc]++; \
        HEAP_COUNTER++; \
      } else { \
        REQ_GL_HEAP_MAP[ret_pc]++; \
        HEAP_COUNTER++; \
      } \
    } else { \
      if (ptr < top && ptr >= bot) { \
        STACK_MAP[ret_pc]++; \
        STACK_COUNTER++; \
      } else if (tx->heapLocalPointers.count(ptr) > 0) { \
        L_HEAP_MAP[ret_pc]++; \
        HEAP_COUNTER++; \
      } else { \
        GL_HEAP_MAP[ret_pc]++; \
        HEAP_COUNTER++; \
      } \
    }

#define READ_PROFILING_CODE PROFILING_CODE(tx->numberOfReads,                   \
                                           tx->numberOfStackReads,              \
                                           tx->numberOfHeapReads,               \
                                           tx->mapStackReadsPCtoCounter,        \
                                           tx->mapGlobalHeapReadsPCtoCounter,   \
                                           tx->mapLocalHeapReadsPCtoCounter,    \
                                           tx->mapReqStackReadsPCtoCounter,     \
                                           tx->mapReqGlobalHeapReadsPCtoCounter,\
                                           tx->mapReqLocalHeapReadsPCtoCounter)
#define WRITE_PROFILING_CODE PROFILING_CODE(tx->numberOfWrites,                   \
                                            tx->numberOfStackWrites,              \
                                            tx->numberOfHeapWrites,               \
                                            tx->mapStackWritesPCtoCounter,        \
                                            tx->mapGlobalHeapWritesPCtoCounter,   \
                                            tx->mapLocalHeapWritesPCtoCounter,    \
                                            tx->mapReqStackWritesPCtoCounter,     \
                                            tx->mapReqGlobalHeapWritesPCtoCounter,\
                                            tx->mapReqLocalHeapWritesPCtoCounter)

#define ITM_READ(TYPE, LSMOD, EXT) \
  ITM_REGPARM TYPE _ITM_##LSMOD##EXT(const TYPE *addr) { \
    READ_PROFILING_CODE \
    return *addr; \
  }

#define ITM_WRITE(TYPE, LSMOD, EXT) \
  void ITM_REGPARM _ITM_##LSMOD##EXT(TYPE *addr, TYPE value) { \
    WRITE_PROFILING_CODE \
    *addr = value; \
  }

ITM_REGPARM ITM_PURE void
_ITM_incrementRequiredReads(void* ptr) {
  last_addr_used = ptr;
  threadDescriptor_t* tx = getThreadDescriptor();
  if ( likely (tx != NULL) ) {
    tx->numberOfRequiredReads++;
  }
}

ITM_REGPARM ITM_PURE void
_ITM_incrementRequiredWrites(void* ptr) {
  last_addr_used = ptr;
  threadDescriptor_t* tx = getThreadDescriptor();
  if ( likely (tx != NULL) ) {
    tx->numberOfRequiredWrites++;
  }
}

static void memtransfer (void* dst, const void* src, size_t size) {
  unsigned char* src_ptr = (unsigned char*)src;
  unsigned char* dst_ptr = (unsigned char*)dst;
  for (size_t i=0; i < size; i++) {
    *dst_ptr = *src_ptr;
    src_ptr++;
    dst_ptr++;
  }
}

#define ITM_VECTOR_READ(TYPE, LSMOD, EXT) \
  ITM_REGPARM TYPE _ITM_##LSMOD##EXT (const TYPE *addr) { \
    READ_PROFILING_CODE \
    TYPE v; \
    memtransfer(&v, addr, sizeof(TYPE)); \
    return v; \
  }

#define ITM_VECTOR_WRITE(TYPE, LSMOD, EXT) \
  ITM_REGPARM void _ITM_##LSMOD##EXT (TYPE *addr, TYPE value) { \
    WRITE_PROFILING_CODE \
    memtransfer(addr, &value, sizeof(TYPE)); \
  }

#define ITM_BARRIERS(TYPE, EXT) \
  ITM_READ(TYPE, R, EXT) \
  ITM_READ(TYPE, RaR, EXT) \
  ITM_READ(TYPE, RaW, EXT) \
  ITM_READ(TYPE, RfW, EXT) \
  ITM_WRITE(TYPE, W, EXT) \
  ITM_WRITE(TYPE, WaR, EXT) \
  ITM_WRITE(TYPE, WaW, EXT) \

#define ITM_VECTOR_BARRIERS(TYPE, EXT) \
  ITM_VECTOR_READ(TYPE, R, EXT) \
  ITM_VECTOR_READ(TYPE, RaR, EXT) \
  ITM_VECTOR_READ(TYPE, RaW, EXT) \
  ITM_VECTOR_READ(TYPE, RfW, EXT) \
  ITM_VECTOR_WRITE(TYPE, W, EXT) \
  ITM_VECTOR_WRITE(TYPE, WaR, EXT) \
  ITM_VECTOR_WRITE(TYPE, WaW, EXT) \

// ABI Section 5.12
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
  ITM_VECTOR_BARRIERS (__m64, M64)
# endif
# ifdef __SSE__
  ITM_VECTOR_BARRIERS (__m128i , M128i)
  ITM_VECTOR_BARRIERS (__m128i , M128ii)
  ITM_VECTOR_BARRIERS (__m128 , M128)
  ITM_VECTOR_BARRIERS (__m128 , M128d)
# endif
# ifdef __AVX__
  ITM_VECTOR_BARRIERS (__m256 , M256i)
  ITM_VECTOR_BARRIERS (__m256 , M256ii)
  ITM_VECTOR_BARRIERS (__m256 , M256)
  ITM_VECTOR_BARRIERS (__m256 , M256d)
# endif
#endif													/* i386 */
#undef ITM_BARRIERS
#undef READ_PROFILING_CODE
#undef WRITE_PROFILING_CODE
#undef PROFILING_CODE
