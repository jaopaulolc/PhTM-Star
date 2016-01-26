#ifndef TM_H
#define TM_H 1

#define MAIN(argc, argv)              int main (int argc, char** argv)
#define MAIN_RETURN(val)              return val

#define GOTO_SIM()                    /* nothing */
#define GOTO_REAL()                   /* nothing */
#define IS_IN_SIM()                   (0)

#define SIM_GET_NUM_CPU(var)          /* nothing */

#define P_MEMORY_STARTUP(numThread)   /* nothing */
#define P_MEMORY_SHUTDOWN()           /* nothing */

#include <htm.h>
#include <msr.h>
#include <pmu.h>
#include <assert.h>
#include <locale.h>

#define TM_ARG                        /* nothing */
#define TM_ARG_ALONE                  /* nothing */
#define TM_ARGDECL                    /* nothing */
#define TM_ARGDECL_ALONE              /* nothing */
#define TM_PURE                       /* nothing */
#define TM_SAFE                       /* nothing */

#if defined(COMMIT_RATE_PROFILING) || defined(TSX_ABORT_PROFILING)
#ifdef COMMIT_RATE_PROFILING

#define TM_STARTUP(numThread)         msrInitialize();                               \
																			pmuStartup(NUMBER_OF_TRANSACTIONS);            \
																			HTM_STARTUP(numThread);                        \
																			pmuAddCustomCounter(0, RTM_TX_STARTED);        \
																			pmuAddCustomCounter(1, RTM_TX_COMMITED);       \
																			pmuAddCustomCounter(2, HLE_TX_STARTED);        \
																			pmuAddCustomCounter(3, HLE_TX_COMMITED)
#endif /* COMMIT_RATE_PROFILING */
#ifdef TSX_ABORT_PROFILING
#define COMMIT_RATE_PROFILING 0
#define TM_STARTUP(numThread)         msrInitialize();                                 \
																			pmuStartup(NUMBER_OF_TRANSACTIONS);              \
																			HTM_STARTUP(numThread);                          \
																			pmuAddCustomCounter(0, TX_ABORT_CONFLICT);       \
																			pmuAddCustomCounter(1, TX_ABORT_CAPACITY);       \
																			pmuAddCustomCounter(2, RTM_TX_ABORT_UNFRIENDLY); \
																			pmuAddCustomCounter(3, RTM_TX_ABORT_OTHER)
#endif /* TSX_ABORT_PROFILING */

#define TM_SHUTDOWN()									setlocale(LC_ALL, ""); \
																			int __numThreads__  = thread_getNumThread(); \
																			int nfixedCounters  = pmuNumberOfFixedCounters(); \
		int ncustomCounters = pmuNumberOfCustomCounters(); \
		int ntotalCounters  = nfixedCounters + ncustomCounters; \
		int nmeasurements = pmuNumberOfMeasurements(); \
		int ii; \
		if(COMMIT_RATE_PROFILING){ \
			printf("\nTx #  | %10s | %19s | %10s | %19s | %24s | %24s | %24s", \
			"RTM START", "RTM COMMIT", "HLE START", "HLE COMMIT", "INSTRUCTIONS", "CYCLES", "CYCLES REF"); \
		} else { \
			printf("\nTx #  | %19s | %19s | %19s | %19s ", \
			"CONFLICT/READ CAP.", "WRITE CAPACITY", "UNFRIENDLY INST.", "OTHER"); \
		} \
		for(ii=0; ii < __numThreads__; ii++){ \
			uint64_t **measurements = pmuGetMeasurements(ii); \
			printf("\nThread %d\n",ii); \
			int i, j; \
			uint64_t total[3] = {0,0,0}; \
			if(COMMIT_RATE_PROFILING){ \
				for(j=ncustomCounters; j < ntotalCounters; j++) \
					for(i=0; i < nmeasurements; i++) \
						total[j-ncustomCounters] += measurements[i][j]; \
			} \
			for(i=0; i < nmeasurements; i++){ \
				printf("Tx %2d",i); \
				if(COMMIT_RATE_PROFILING){ \
					for(j=0; j < ncustomCounters; j++){ \
						if(j && j % 2){ \
							printf(" | %'10lu ",measurements[i][j]); \
							printf("(%'6.2lf)", 100.0*((double)measurements[i][j]/(double)measurements[i][j-1])); \
						} else printf(" | %'10lu",measurements[i][j]); \
					} \
				} else { \
					uint64_t sum = 0; \
					for(j=0; j < ncustomCounters; j++){ sum += measurements[i][j]; } \
					for(j=0; j < ncustomCounters; j++){ \
						printf(" | %'10lu ",measurements[i][j]); \
						printf("(%'6.2lf)", 100.0*((double)measurements[i][j]/(double)sum)); \
					} \
				} \
				if(COMMIT_RATE_PROFILING){ \
					for(j=ncustomCounters; j < ntotalCounters; j++){ \
						printf(" | %'15lu ",measurements[i][j]); \
						printf("(%'6.2lf)", 100.0*((double)measurements[i][j]/(double)total[j-ncustomCounters])); \
					} \
				} \
				printf("\n"); \
			} \
		} \
		printf("\n|=========================%46s%s%46s=========================|\n","", "END OF REPORT", " "); \
																			HTM_SHUTDOWN(); \
																			pmuShutdown(); \
																			msrTerminate()

#define TM_THREAD_ENTER()             long __threadId__ = thread_getId();\
																			set_affinity(__threadId__); \
																			HTM_THREAD_ENTER(__threadId__)
#define TM_THREAD_EXIT()              HTM_THREAD_EXIT()


#define TM_BEGIN()                    pmuStartCounting(__threadId__, __COUNTER__); \
																			TX_START()
#define TM_BEGIN_RO()                 pmuStartCounting(__threadId__, __COUNTER__); \
																			TX_START()
#define TM_END()                      TX_END(); \
																			pmuStopCounting(__threadId__)
#else /* NO PROFILING */

#define TM_STARTUP(numThread)         msrInitialize(); \
																			HTM_STARTUP(numThread)
#define TM_SHUTDOWN()                 HTM_SHUTDOWN(); \
																			msrTerminate()

#define TM_THREAD_ENTER()             long __threadId__ = thread_getId();\
																			set_affinity(__threadId__); \
																			HTM_THREAD_ENTER(__threadId__)
#define TM_THREAD_EXIT()              HTM_THREAD_EXIT()

#define TM_BEGIN()                    TX_START()
#define TM_BEGIN_RO()                 TX_START()
#define TM_END()                      TX_END();

#endif /* NO PROFILING */

#define TM_RESTART()                  _xabort(0xab)
#define TM_EARLY_RELEASE(var)         /* nothing */


#define SEQ_MALLOC(size)              malloc(size)
#define SEQ_FREE(ptr)                 free(ptr)
#define P_MALLOC(size)                malloc(size)
#define P_FREE(ptr)                   free(ptr)
#define TM_MALLOC(size)               malloc(size)
#define TM_FREE(ptr)                  free(ptr)


#define TM_SHARED_READ(var)           (var)
#define TM_SHARED_READ_P(var)         (var)
#define TM_SHARED_READ_F(var)         (var)

#define TM_SHARED_WRITE(var, val)     var = val
#define TM_SHARED_WRITE_P(var, val)   var = val
#define TM_SHARED_WRITE_F(var, val)   var = val

#define TM_LOCAL_WRITE(var, val)      var = val
#define TM_LOCAL_WRITE_P(var, val)    var = val
#define TM_LOCAL_WRITE_F(var, val)    var = val

#define TM_IFUNC_DECL                 /* nothing */
#define TM_IFUNC_CALL1(r, f, a1)      r = f(a1)
#define TM_IFUNC_CALL2(r, f, a1, a2)  r = f((a1), (a2))

#endif /* TM_H */

