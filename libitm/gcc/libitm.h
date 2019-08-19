#ifndef _LIBITM_H
#define _LIBITM_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <immintrin.h>

#ifdef __cplusplus
extern "C" {
#endif

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#define NO_INLINE __attribute__((noinline))
#define UNUSED __attribute__((unused))
#define _ITM_noTransactionId 1

#ifdef __i386__
	/* Only for 32-bit x86.  */
# define ITM_REGPARM	__attribute__((regparm(2)))
#else
# define ITM_REGPARM
#endif

#define _ITM_MALLOC __attribute__((__malloc__))
#define _ITM_NORETURN	__attribute__((noreturn))
#define ITM_PURE __attribute__((transaction_pure))

/* Results from inTransaction */
typedef enum {
	outsideTransaction = 0,			/* So “if (inTransaction(td))” works */
	inRetryableTransaction,
	inIrrevocableTransaction
} _ITM_howExecuting;

// ABI Section 5.6
typedef struct {
	uint32_t reserved_1;
	uint32_t flags;
	uint32_t reserved_2;
	uint32_t reserved_3;
	const char *psource;
} _ITM_srcLocation;

// ABI Section 5.2
#define _ITM_VERSION "0.9 (May 6, 2011)"
#define _ITM_VERSION_NO 90
int ITM_REGPARM _ITM_versionCompatible (int);
const char *ITM_REGPARM _ITM_libraryVersion (void);

// ABI Section 5.3
void ITM_REGPARM _ITM_NORETURN
	_ITM_error (const _ITM_srcLocation * srcLoc UNUSED, int errorCode UNUSED);

// ABI Section 5.7
// Values to describe properties of code, passed in to startTransaction
typedef enum {
	pr_instrumentedCode = 0x0001,
	pr_uninstrumentedCode = 0x0002,
	pr_multiwayCode = pr_instrumentedCode | pr_uninstrumentedCode,
	pr_hasNoXMMUpdate = 0x0004,
	pr_hasNoAbort = 0x0008,
	pr_hasNoIrrevocable = 0x0020,
	pr_doesGoIrrevocable = 0x0040,
	pr_aWBarriersOmitted = 0x0100,
	pr_RaRBarriersOmitted = 0x0200,
	pr_undoLogCode = 0x0400,
	pr_preferUninstrumented = 0x0800,
	pr_exceptionBlock = 0x1000,
	pr_readOnly = 0x4000,
	pr_hasNoRetry = 0x100000,
	pr_hasElse = 0x200000,
	pr_hasNoSimpleReads = 0x400000,
	reserved = 0x10000000
} _ITM_codeProperties;

// Result from beginTransaction that describes what actions to take.
typedef enum {
	a_runInstrumentedCode = 0x01,
	a_runUninstrumentedCode = 0x02,
	a_saveLiveVariables = 0x04,
	a_restoreLiveVariables = 0x08,
	a_abortTransaction = 0x10,
} _ITM_actions;

#include "config/arch.h" // _ITM_beginTransaction && jmpbuf_t

extern uint32_t _ITM_beginTransaction (uint32_t codeProperties, jmpbuf_t *jmpbuf) ITM_REGPARM;

// ABI Section 5.8
/* Values used as arguments to abort. */
typedef enum {
	userAbort = 1,
	userRetry = 2,
	TMConflict = 4,
	exceptionBlockAbort = 8
} _ITM_abortReason;

extern void _ITM_NORETURN
	_ITM_abortTransaction (_ITM_abortReason abortReason) ITM_REGPARM;

extern void _ITM_rollbackTransaction () ITM_REGPARM;

extern void _ITM_commitTransaction () ITM_REGPARM;

extern bool _ITM_tryCommitTransaction () ITM_REGPARM;

// GNU C/C++ TM Constructs specific memory (de)allocation functions
extern void *_ITM_malloc (size_t size) _ITM_MALLOC ITM_PURE;
extern void *_ITM_calloc (size_t nmemb, size_t size) _ITM_MALLOC ITM_PURE;
extern void _ITM_free (void *) ITM_PURE;
// ??? These are not yet in the official spec; still work-in-progress.
extern void *_ITM_getTMCloneOrIrrevocable (void *) ITM_REGPARM;
extern void *_ITM_getTMCloneSafe (void *) ITM_REGPARM;
extern void _ITM_registerTMCloneTable (void *, size_t);
extern void _ITM_deregisterTMCloneTable (void *);


// ABI Section 5.12 
#define ITM_BARRIERS(TYPE, EXT) \
	extern TYPE _ITM_R##EXT(const TYPE *addr) ITM_REGPARM; \
	extern TYPE _ITM_RaR##EXT(const TYPE *addr) ITM_REGPARM; \
	extern TYPE _ITM_RaW##EXT(const TYPE *addr) ITM_REGPARM; \
	extern TYPE _ITM_RfW##EXT(const TYPE *addr) ITM_REGPARM; \
	extern void _ITM_W##EXT(TYPE *addr, TYPE value) ITM_REGPARM; \
	extern void _ITM_WaR##EXT(TYPE *addr, TYPE value) ITM_REGPARM; \
	extern void _ITM_WaW##EXT(TYPE *addr, TYPE value) ITM_REGPARM;

ITM_BARRIERS (uint8_t, U1)
ITM_BARRIERS (uint16_t, U2)
ITM_BARRIERS (uint32_t, U4)
ITM_BARRIERS (uint64_t, U8)
ITM_BARRIERS (float, F)
ITM_BARRIERS (double, D)
ITM_BARRIERS (long double, E)
ITM_BARRIERS (float _Complex, CF)
ITM_BARRIERS (double _Complex, CD)
ITM_BARRIERS (long double _Complex, CE)
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

// ABI Section 5.13
extern void _ITM_memcpyRnWt (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memcpyRnWtaR (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memcpyRnWtaW (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memcpyRtWn (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memcpyRtWt (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memcpyRtWtaR (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memcpyRtWtaW (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memcpyRtaRWn (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memcpyRtaRWt (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memcpyRtaRWtaR (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memcpyRtaRWtaW (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memcpyRtaWWn (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memcpyRtaWWt (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memcpyRtaWWtaR (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memcpyRtaWWtaW (void *, const void *, size_t) ITM_REGPARM;

// ABI Section 5.14
extern void _ITM_memmoveRnWt (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memmoveRnWtaR (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memmoveRnWtaW (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memmoveRtWn (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memmoveRtWt (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memmoveRtWtaR (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memmoveRtWtaW (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memmoveRtaRWn (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memmoveRtaRWt (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memmoveRtaRWtaR (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memmoveRtaRWtaW (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memmoveRtaWWn (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memmoveRtaWWt (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memmoveRtaWWtaR (void *, const void *, size_t) ITM_REGPARM;
extern void _ITM_memmoveRtaWWtaW (void *, const void *, size_t) ITM_REGPARM;

// ABI Section 5.15
extern void _ITM_memsetW (void *, int, size_t) ITM_REGPARM;
extern void _ITM_memsetWaR (void *, int, size_t) ITM_REGPARM;
extern void _ITM_memsetWaW (void *, int, size_t) ITM_REGPARM;

// ABI Section 5.16
extern void _ITM_LB (const void *, size_t) ITM_REGPARM;
#define ITM_LOG(TYPE, EXT) \
	extern void _ITM_L##EXT (const TYPE *) ITM_REGPARM;

ITM_LOG (uint8_t, U1)
ITM_LOG (uint16_t, U2)
ITM_LOG (uint32_t, U4)
ITM_LOG (uint64_t, U8)
ITM_LOG (float, F)
ITM_LOG (double, D)
ITM_LOG (long double, E)
ITM_LOG (float _Complex, CF)
ITM_LOG (double _Complex, CD)
ITM_LOG (long double _Complex, CE)
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

#ifdef __cplusplus
}
#endif

#endif													/* _LIBITM_H */
