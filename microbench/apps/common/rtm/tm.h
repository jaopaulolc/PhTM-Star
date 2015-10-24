#ifndef TM_H
#define TM_H 1

#include <rtm.h>
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

#if defined(PROFILING)

#define TM_INIT(nThreads)       TSX_STARTUP(nThreads);                          \
                                msrInitialize();                                \
																pmuStartup(1);                                  \
															  pmuAddCustomCounter(0,RTM_TX_STARTED);          \
															  pmuAddCustomCounter(1,RTM_TX_COMMITED);         \
															  pmuAddCustomCounter(2,TX_ABORT_CONFLICT);       \
															  pmuAddCustomCounter(3,TX_ABORT_CAPACITY)
#define TM_EXIT(nThreads)       TSX_SHUTDOWN();                                                            \
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
		uint64_t conflicts = eventCount[2];                                                                    \
		uint64_t capacity  = eventCount[3];                                                                    \
		printf("#commits   : %lu\n", eventCount[1]);                                                           \
		printf("#conflicts : %lu (%f)\n", conflicts, 100.0*((float)conflicts/(float)eventCount[0]) );          \
		printf("#capacity  : %lu (%f)\n", capacity, 100.0*((float)capacity/(float)eventCount[0]));             \
																			pmuShutdown();                                                       \
																			msrTerminate()
#if 0
		printf("#starts    : %lu\n", eventCount[0]);                                                           \
		printf("#commits   : %lu\n", eventCount[1]);                                                           \
		printf("#conflicts : %lu (%f)\n", conflicts, 100.0*((float)conflicts/(float)eventCount[0]) );          \
		printf("#capacity  : %lu (%f)\n", eventCount[3], 100.0*((float)eventCount[3]/(float)eventCount[0]));
#endif

#define TM_INIT_THREAD(tid)           TX_INIT(tid); set_affinity(tid); \
																			pmuStartCounting(tid,0)
#define TM_EXIT_THREAD(tid)           pmuStopCounting(tid)

#else

#define TM_INIT(nThreads)             TSX_STARTUP(nThreads)
#define TM_EXIT(nThreads)             TSX_SHUTDOWN()

#define TM_INIT_THREAD(tid)           TX_INIT(tid); set_affinity(tid)
#define TM_EXIT_THREAD                /* nothing */

#endif

#define TM_START(tid,ro)              TX_START()
#define TM_START_TS(tid,ro)           TX_START()
#define TM_COMMIT                     TX_END()

#define TM_RESTART()                  _xabort(0xab)


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

