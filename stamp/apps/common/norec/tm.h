#ifndef TM_H
#define TM_H 1

#include <stdio.h>

#define MAIN(argc, argv)              int main (int argc, char** argv)
#define MAIN_RETURN(val)              return val

#define GOTO_SIM()                    /* nothing */
#define GOTO_REAL()                   /* nothing */
#define IS_IN_SIM()                   (0)

#define SIM_GET_NUM_CPU(var)          /* nothing */

#define P_MEMORY_STARTUP(numThread)   /* nothing */
#define P_MEMORY_SHUTDOWN()           /* nothing */

#include <msr.h>
#include <pmu.h>

#include <string.h>
/* The API is specific for STAMP. */
#include <api/api.hpp>
#include <thread.h>

/* These macro are already defined in library.hpp. */
#undef  TM_ARG
#undef  TM_ARG_ALONE
#undef  TM_BEGIN
#undef  TM_END

#define TM_ARG                        STM_SELF,
#define TM_ARG_ALONE                  STM_SELF
#define TM_ARGDECL                    STM_THREAD_T* TM_ARG
#define TM_ARGDECL_ALONE              STM_THREAD_T* TM_ARG_ALONE
#define TM_SAFE                       /* nothing */
#define TM_PURE                       /* nothing */

#ifdef PROFILING

extern __thread long __txId__;
extern long **__numCommits;
extern long **__numAborts;
extern long **__readSetSize;
extern long **__writeSetSize;

#define TM_STARTUP(numThread)	msrInitialize();                    \
															pmuStartup(NUMBER_OF_TRANSACTIONS); \
															STM_STARTUP(numThread);             \
										{ int i;                                                    \
											__numCommits   = (long**)malloc(numThread*sizeof(long*)); \
											__numAborts    = (long**)malloc(numThread*sizeof(long*)); \
											__readSetSize  = (long**)malloc(numThread*sizeof(long*)); \
											__writeSetSize = (long**)malloc(numThread*sizeof(long*)); \
											for(i=0; i < numThread; i++){                             \
												__numCommits[i]   = (long *)calloc(NUMBER_OF_TRANSACTIONS,sizeof(long)); \
												__numAborts[i]    = (long *)calloc(NUMBER_OF_TRANSACTIONS,sizeof(long)); \
												__readSetSize[i]  = (long *)calloc(NUMBER_OF_TRANSACTIONS,sizeof(long)); \
												__writeSetSize[i] = (long *)calloc(NUMBER_OF_TRANSACTIONS,sizeof(long)); \
											}                                                                          \
										}

#define TM_SHUTDOWN()	STM_SHUTDOWN(); \
					{ int i;                                                             \
						int __numThreads__  = thread_getNumThread();                       \
						int ncustomCounters = pmuNumberOfCustomCounters();                 \
						int nfixedCounters  = pmuNumberOfFixedCounters();                  \
						int ntotalCounters  = nfixedCounters + ncustomCounters;            \
						int nmeasurements   = pmuNumberOfMeasurements();                   \
						printf("Tx #  | %10s | %19s | %20s | %20s | %21s | %21s | %20s\n",               \
						"STARTS", "COMMITS", "READS", "WRITES", "INSTRUCTIONS", "CYCLES", "CYCLES REF"); \
						for(i=0; i < __numThreads__; i++) {                                \
							uint64_t **measurements = pmuGetMeasurements(i);                 \
							int j, k;                                                        \
							uint64_t total[3] = {0,0,0};                                     \
							for(j=ncustomCounters; j < ntotalCounters; j++)                  \
								for(k=0; k < nmeasurements; k++)                               \
									total[j-ncustomCounters] += measurements[k][j];              \
							printf("Thread %d\n",i);                                         \
					  	for(j=0; j < nmeasurements; j++) {                               \
								long numCommits = __numCommits[i][j];                          \
								long numStarts  = numCommits + __numAborts[i][j];              \
								printf("Tx %2d | %10ld | %10ld (%6.2lf) | %9ld (%8.2lf) | %9ld (%8.2lf) | %12lu (%6.2lf) | %12lu (%6.2lf) | %11lu (%6.2lf)\n"   \
								, j, numStarts, numCommits, 1.0e2*((double)numCommits/(double)numStarts), __readSetSize[i][j]                                   \
								, 1.0e0*((double)__readSetSize[i][j]/(double)numStarts), __writeSetSize[i][j]                                                   \
								, 1.0e0*((double)__writeSetSize[i][j]/(double)numStarts), measurements[j][ncustomCounters+0]                                    \
								, 1.0e2*((double)measurements[j][ncustomCounters+0]/(double)total[0]), measurements[j][ncustomCounters+1]                       \
								, 1.0e2*((double)measurements[j][ncustomCounters+1]/(double)total[1]), measurements[j][ncustomCounters+2]                       \
								, 1.0e2*((double)measurements[j][ncustomCounters+2]/(double)total[2]));                                                         \
							}                                                                \
							printf("\n");                                                    \
							free(__numCommits[i]);  free(__numAborts[i]);                    \
							free(__readSetSize[i]); free(__writeSetSize[i]);                 \
						}                                                                  \
						free(__numCommits);  free(__numAborts);                            \
						free(__readSetSize); free(__writeSetSize);                         \
					}                                                                    \
					pmuShutdown();                                                       \
					msrTerminate()

#else /* NO PROFILING */

#define TM_STARTUP(numThread)	msrInitialize();                    \
															STM_STARTUP(numThread)

#define TM_SHUTDOWN()	STM_SHUTDOWN(); \
											msrTerminate()
#endif /* NO PROFILING */

#define TM_THREAD_ENTER()    long __threadId__ = thread_getId(); \
														 TM_ARGDECL_ALONE = STM_NEW_THREAD(); \
                             STM_INIT_THREAD(TM_ARG_ALONE, thread_getId()); \
														 set_affinity(__threadId__)

#define TM_THREAD_EXIT()              STM_FREE_THREAD(TM_ARG_ALONE)

#define P_MALLOC(size)                malloc(size)
#define P_FREE(ptr)                   free(ptr)
#define SEQ_MALLOC(size)              malloc(size)
#define SEQ_FREE(ptr)                 free(ptr)
#define TM_MALLOC(size)               TM_ALLOC(size)
/* TM_FREE(ptr) is already defined in the file interface. */

#ifdef PROFILING

#define TM_BEGIN()                    __txId__ = __COUNTER__;                   \
																			pmuStartCounting(__threadId__, __txId__); \
																			STM_BEGIN_WR()
#define TM_BEGIN_RO()                 __txId__ = __COUNTER__;                   \
																			pmuStartCounting(__threadId__, __txId__); \
																			STM_BEGIN_RD()
#define TM_END()                      STM_END();                    \
																			pmuStopCounting(__threadId__)

#else /* NO PROFILING */

#define TM_BEGIN()                    STM_BEGIN_WR()
#define TM_BEGIN_RO()                 STM_BEGIN_RD()
#define TM_END()                      STM_END()

#endif /* NO PROFILING */

#define TM_RESTART()                  STM_RESTART()

#define TM_EARLY_RELEASE(var)         /* nothing */

#define STMREAD                       stm::stm_read
#define STMWRITE                      stm::stm_write

#define TM_SHARED_READ(var)           STMREAD(&var, (stm::TxThread*)STM_SELF)
#define TM_SHARED_READ_P(var)         STMREAD(&var, (stm::TxThread*)STM_SELF)
#define TM_SHARED_READ_F(var)         STMREAD(&var, (stm::TxThread*)STM_SELF)

#define TM_SHARED_WRITE(var, val)     STMWRITE(&var, val, (stm::TxThread*)STM_SELF)
#define TM_SHARED_WRITE_P(var, val)   STMWRITE(&var, val, (stm::TxThread*)STM_SELF)
#define TM_SHARED_WRITE_F(var, val)   STMWRITE(&var, val, (stm::TxThread*)STM_SELF)

#define TM_LOCAL_WRITE(var, val)      STM_LOCAL_WRITE_L(var, val)
#define TM_LOCAL_WRITE_P(var, val)    STM_LOCAL_WRITE_P(var, val)
#define TM_LOCAL_WRITE_F(var, val)    STM_LOCAL_WRITE_F(var, val)
#define TM_LOCAL_WRITE_D(var, val)    STM_LOCAL_WRITE_D(var, val)

#define TM_IFUNC_DECL                 /* nothing */
#define TM_IFUNC_CALL1(r, f, a1)      r = f(a1)
#define TM_IFUNC_CALL2(r, f, a1, a2)  r = f((a1), (a2))

#endif /* TM_H */

