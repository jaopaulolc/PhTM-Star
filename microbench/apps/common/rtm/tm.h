#ifndef TM_H
#define TM_H 1

#include <htm.h>
#include <msr.h>
#include <pmu.h>
#include <locale.h>
#include <assert.h>

#define TM_ARG                        /* nothing */
#define TM_ARG_ALONE                  /* nothing */
#define TM_ARGDECL                    /* nothing */
#define TM_ARGDECL_ALONE              /* nothing */
#define TM_PURE                       /* nothing */
#define TM_SAFE                       /* nothing */
#define TM_CALLABLE                   /* nothing */

#if defined(TSX_ABORT_PROFILING)

#define TM_INIT(nThreads)       HTM_STARTUP(nThreads);                          \
                                msrInitialize();                                \
																pmuStartup(1);                                  \
															  pmuAddCustomCounter(0,RTM_TX_ABORTED);          \
															  pmuAddCustomCounter(1,RTM_TX_COMMITED);         \
															  pmuAddCustomCounter(2,TX_ABORT_CONFLICT);       \
															  pmuAddCustomCounter(3,TX_ABORT_CAPACITY)        
#define TM_EXIT(nThreads)       HTM_SHUTDOWN();                                                            \
    {                                                                                                      \
		setlocale(LC_ALL, "");                                                                                 \
		int ncustomCounters = pmuNumberOfCustomCounters();                                                     \
		uint64_t eventCount[ncustomCounters];                                                                  \
		memset(eventCount, 0, sizeof(uint64_t)*ncustomCounters);                                               \
		int ii;                                                                                                \
		for(ii=0; ii < nThreads; ii++){                                                                        \
			uint64_t **measurements = pmuGetMeasurements(ii);                                                    \
			long j;                                                                                              \
			for(j=0; j < ncustomCounters; j++){                                                                  \
				eventCount[j] += measurements[0][j];                                                               \
			}                                                                                                    \
		}                                                                                                      \
		uint64_t aborts  = eventCount[0];                                                                      \
		uint64_t commits = eventCount[1];                                                                      \
		uint64_t starts  = commits + aborts;                                                                   \
		uint64_t conflicts = eventCount[2];                                                                    \
		uint64_t capacity  = eventCount[3];                                                                    \
		printf("#starts    : %12lu\n", starts);                                                                \
		printf("#commits   : %12lu %6.2f\n", commits, 100.0*((float)commits/(float)starts));                   \
		printf("#aborts    : %12lu %6.2f\n", aborts, 100.0*((float)aborts/(float)starts));                     \
		printf("#conflicts : %12lu\n", conflicts);                                                             \
		printf("#capacity  : %12lu\n", capacity);                                                              \
		}                                                                                                      \
																			pmuShutdown();                                                       \
																			msrTerminate()

#define TM_INIT_THREAD(tid)           set_affinity(tid); HTM_THREAD_ENTER(tid); \
																			pmuStartCounting(tid,0)
#define TM_EXIT_THREAD(tid)           pmuStopCounting(tid); HTM_THREAD_EXIT()

#else /* ! TSX_ABORT_PROFILING */

#define TM_INIT(nThreads)             HTM_STARTUP(nThreads)
#define TM_EXIT(nThreads)             HTM_SHUTDOWN()

#define TM_INIT_THREAD(tid)           set_affinity(tid); HTM_THREAD_ENTER(tid)
#define TM_EXIT_THREAD(tid)           HTM_THREAD_EXIT()

#endif /* ! TSX_ABORT_PROFILING */

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

