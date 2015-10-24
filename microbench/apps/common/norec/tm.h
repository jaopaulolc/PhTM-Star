#ifndef TM_H
#define TM_H 1

#include <stdio.h>
#include <string.h>
#include <api/api.hpp>
#include <stm/txthread.hpp>

#undef TM_ARG
#undef TM_ARG_ALONE 

#define TM_SAFE                       /* nothing */
#define TM_PURE                       /* nothing */
#define TM_CALLABLE                   /* nothing */

#define TM_ARG                        /* nothing */
#define TM_ARG_ALONE                  /* nothing */
#define TM_ARGDECL                    /* nothing */
#define TM_ARGDECL_ALONE              /* nothing */


#define TM_INIT(nThreads)	            stm::sys_init(NULL)

#define TM_EXIT(nThreads)             stm::sys_shutdown()

#define TM_INIT_THREAD(tid)           stm::thread_init(); \
														          set_affinity(tid)

#define TM_EXIT_THREAD(tid)           stm::thread_shutdown()

#define TM_MALLOC(size)               TM_ALLOC(size)
/* TM_FREE(ptr) is already defined in the file interface. */
#define TM_FREE2(ptr,size)            TM_FREE(ptr)

#define TM_START(tid, ro)                          \
    {                                              \
    stm::TxThread* tx = (stm::TxThread*)stm::Self; \
    jmp_buf _jmpbuf;                               \
    uint32_t abort_flags = setjmp(_jmpbuf);        \
    stm::begin(tx, &_jmpbuf, abort_flags);         \
    CFENCE;                                        \
		{

#define TM_COMMIT         \
    }                     \
    stm::commit(tx);      \
		}

#define TM_RESTART()                  stm::restart()

#define TM_LOAD(addr)                 stm::stm_read((long int*)addr, (stm::TxThread*)stm::Self)
#define TM_STORE(addr, val)           stm::stm_write((long int*)addr,(long int)val, (stm::TxThread*)stm::Self)

#define TM_SHARED_READ(var)          TM_LOAD(&(var))
#define TM_SHARED_READ_P(var)         TM_LOAD(&(var))

#define TM_SHARED_WRITE(var, val)    TM_STORE(&(var),val)
#define TM_SHARED_WRITE_P(var, val)   TM_STORE(&(var),val)

#endif /* TM_H */
