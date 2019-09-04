#ifndef TM_H
#define TM_H 1

#include <stdio.h>
#include <string.h>

#include <libitm.h>

#undef TM_ARG
#undef TM_ARG_ALONE

#define TM_SAFE                       __attribute__((transaction_safe))
#define TM_PURE                       __attribute__((transaction_pure))
#define TM_CALLABLE                   /* nothing */

#define TM_ARG                        /* nothing */
#define TM_ARG_ALONE                  /* nothing */
#define TM_ARGDECL                    /* nothing */
#define TM_ARGDECL_ALONE              /* nothing */

#define TM_INIT(nThreads) /* nothing */

#define TM_EXIT(nThreads) /* nothing */

#define TM_INIT_THREAD(tid) /* nothing */

#define TM_EXIT_THREAD(tid) /* nothing */

#define TM_MALLOC(size)               malloc(size)
#define TM_FREE(ptr)                  free(ptr)
#define TM_FREE2(ptr,size)            free(ptr)

#define TM_START(tid, ro)  __transaction_atomic {
#define TM_COMMIT          }

#define TM_RESTART() _ITM_abortTransaction(2)

extern void _ITM_incrementRequiredReads(void* ptr) TM_PURE;
extern void _ITM_incrementRequiredWrites(void* ptr) TM_PURE;

#define TM_LOAD(addr)        (_ITM_incrementRequiredReads((void*)(addr)),*(addr))
#define TM_STORE(addr, val)  (_ITM_incrementRequiredWrites((void*)(addr)),*(addr)=(val))

#define TM_SHARED_READ(var)          TM_LOAD(&(var))
#define TM_SHARED_READ_P(var)        TM_LOAD(&(var))

#define TM_SHARED_WRITE(var, val)    TM_STORE(&(var),val)
#define TM_SHARED_WRITE_P(var, val)  TM_STORE(&(var),val)

#ifdef exit
#undef exit
#endif
extern void exit(int status) TM_PURE;
#ifdef perror
#undef perror
#endif
extern void perror(const char* s) TM_PURE;
#ifdef erand48
#undef erand48
#endif
extern double erand48(unsigned short s[3]) TM_PURE;

#endif /* TM_H */
