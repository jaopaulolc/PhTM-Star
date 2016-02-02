#ifndef TM_H
#define TM_H 1

#include <htm.h>
#include <locale.h>
#include <assert.h>

#define TM_ARG                        /* nothing */
#define TM_ARG_ALONE                  /* nothing */
#define TM_ARGDECL                    /* nothing */
#define TM_ARGDECL_ALONE              /* nothing */
#define TM_PURE                       /* nothing */
#define TM_SAFE                       /* nothing */
#define TM_CALLABLE                   /* nothing */

#define TM_INIT(nThreads)             HTM_STARTUP(nThreads)
#define TM_EXIT(nThreads)             HTM_SHUTDOWN()

#define TM_INIT_THREAD(tid)           set_affinity(tid); HTM_THREAD_ENTER(tid) 
#define TM_EXIT_THREAD(tid)           HTM_THREAD_EXIT()

#define TM_START(tid,ro)              TX_START()
#define TM_START_TS(tid,ro)           TX_START()
#define TM_COMMIT                     TX_END()

#define TM_RESTART()                  htm_abort()


#define TM_MALLOC(size)               malloc(size)
#define TM_FREE(ptr)                  free(ptr)
#define TM_FREE2(ptr,size)            free(ptr)


#define TM_LOAD(addr)                 (*addr)
#define TM_STORE(addr, value)         (*addr) = value

#define TM_SHARED_READ(var)            (var)
#define TM_SHARED_READ_P(var)          (var)

#define TM_SHARED_WRITE(var, val)      var = val
#define TM_SHARED_WRITE_P(var, val)    var = val

#endif /* TM_H */
