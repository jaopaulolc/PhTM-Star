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

#if defined(__x86_64__) || defined(__i386)
#include <msr.h>
#include <pmu.h>
#else
#define msrInitialize()         			/* nothing */
#define msrTerminate()          			/* nothing */
#endif

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

#define P_MALLOC(size)                malloc(size)
#define P_FREE(ptr)                   free(ptr)
#define SEQ_MALLOC(size)              malloc(size)
#define SEQ_FREE(ptr)                 free(ptr)
#define TM_MALLOC(size)               TM_ALLOC(size)
/* TM_FREE(ptr) is already defined in the file interface. */

#if defined(COMMIT_RATE_PROFILING) || defined(RW_SET_PROFILING)

extern __thread long __txId__;
extern uint64_t **__stm_commits;
extern uint64_t **__stm_aborts;

extern void norecInitThreadCommits(uint64_t* addr);
extern void norecInitThreadAborts(uint64_t* addr);

#ifdef COMMIT_RATE_PROFILING
extern uint64_t **__readSetSize;
extern uint64_t **__writeSetSize;

#define TM_STARTUP(numThread)	msrInitialize();                                                     \
															pmuStartup(NUMBER_OF_TRANSACTIONS);                                  \
															STM_STARTUP(numThread);                                              \
										{ int i;                                                                       \
											__stm_commits = (uint64_t **)malloc(sizeof(uint64_t *)*numThread);           \
								    	__stm_aborts  = (uint64_t **)malloc(sizeof(uint64_t *)*numThread);           \
											__readSetSize  = (uint64_t**)malloc(numThread*sizeof(uint64_t*));                  \
											__writeSetSize = (uint64_t**)malloc(numThread*sizeof(uint64_t*));                  \
											for(i=0; i < numThread; i++){                                                      \
												__stm_commits[i] = (uint64_t*)calloc(NUMBER_OF_TRANSACTIONS,sizeof(uint64_t));   \
												__stm_aborts[i]  = (uint64_t*)calloc(NUMBER_OF_TRANSACTIONS,sizeof(uint64_t));   \
												__readSetSize[i]  = (uint64_t *)calloc(NUMBER_OF_TRANSACTIONS,sizeof(uint64_t)); \
												__writeSetSize[i] = (uint64_t *)calloc(NUMBER_OF_TRANSACTIONS,sizeof(uint64_t)); \
											}                                                                                  \
										}

#define TM_SHUTDOWN()	STM_SHUTDOWN(); \
					{ uint64_t i;                                                             \
						uint64_t __numThreads__  = thread_getNumThread();                       \
						uint64_t ncustomCounters = pmuNumberOfCustomCounters();                 \
						uint64_t nfixedCounters  = pmuNumberOfFixedCounters();                  \
						uint64_t ntotalCounters  = nfixedCounters + ncustomCounters;            \
						uint64_t nmeasurements   = pmuNumberOfMeasurements();                   \
						printf("Tx #  | %10s | %19s | %20s | %20s | %21s | %21s | %20s\n",               \
						"STARTS", "COMMITS", "READS", "WRITES", "INSTRUCTIONS", "CYCLES", "CYCLES REF"); \
						for(i=0; i < __numThreads__; i++) {                                \
							uint64_t **measurements = pmuGetMeasurements(i);                 \
							uint64_t j, k;                                                   \
							uint64_t total[3] = {0,0,0};                                     \
							for(j=ncustomCounters; j < ntotalCounters; j++)                  \
								for(k=0; k < nmeasurements; k++)                               \
									total[j-ncustomCounters] += measurements[k][j];              \
							printf("Thread %lu\n",i);                                         \
					  	for(j=0; j < nmeasurements; j++) {                               \
								uint64_t numCommits = __stm_commits[i][j];                     \
								uint64_t numStarts  = numCommits + __stm_aborts[i][j];         \
								printf("Tx %2lu | %10lu | %10lu (%6.2lf) | %9ld (%8.2lf) | %9ld (%8.2lf) | %12lu (%6.2lf) | %12lu (%6.2lf) | %11lu (%6.2lf)\n"  \
								, j, numStarts, numCommits, 1.0e2*((double)numCommits/(double)numStarts), __readSetSize[i][j]                                   \
								, 1.0e0*((double)__readSetSize[i][j]/(double)numStarts), __writeSetSize[i][j]                                                   \
								, 1.0e0*((double)__writeSetSize[i][j]/(double)numStarts), measurements[j][ncustomCounters+0]                                    \
								, 1.0e2*((double)measurements[j][ncustomCounters+0]/(double)total[0]), measurements[j][ncustomCounters+1]                       \
								, 1.0e2*((double)measurements[j][ncustomCounters+1]/(double)total[1]), measurements[j][ncustomCounters+2]                       \
								, 1.0e2*((double)measurements[j][ncustomCounters+2]/(double)total[2]));                                                         \
							}                                                                \
							printf("\n");                                                    \
							free(__stm_commits[i]);  free(__stm_aborts[i]);                  \
							free(__readSetSize[i]); free(__writeSetSize[i]);                 \
						}                                                                  \
						free(__stm_commits);  free(__stm_aborts);                          \
						free(__readSetSize); free(__writeSetSize);                         \
					}                                                                    \
					pmuShutdown();                                                       \
					msrTerminate()

#endif /* COMMIT_RATE_PROFILING */

#ifdef RW_SET_PROFILING
#define MAX_TRANSACTIONS 40000000L
extern uint64_t **__counter;
extern uint64_t ***__readSetSize;
extern uint64_t ***__writeSetSize;

#define TM_STARTUP(numThread)	msrInitialize();                                                     \
															pmuStartup(NUMBER_OF_TRANSACTIONS);                                  \
															STM_STARTUP(numThread);                                              \
										{ int i;                                                                       \
											__stm_commits   = (uint64_t**)malloc(numThread*sizeof(uint64_t*));                   \
											__stm_aborts    = (uint64_t**)malloc(numThread*sizeof(uint64_t*));                   \
											__counter      = (uint64_t**)malloc(numThread*sizeof(uint64_t*));                    \
											__readSetSize  = (uint64_t***)malloc(numThread*sizeof(uint64_t**));                  \
											__writeSetSize = (uint64_t***)malloc(numThread*sizeof(uint64_t**));                  \
											for(i=0; i < numThread; i++){                                                \
												__counter[i]      = (uint64_t *)calloc(NUMBER_OF_TRANSACTIONS,sizeof(uint64_t));   \
												__stm_commits[i]   = (uint64_t *)calloc(NUMBER_OF_TRANSACTIONS,sizeof(uint64_t));  \
												__stm_aborts[i]    = (uint64_t *)calloc(NUMBER_OF_TRANSACTIONS,sizeof(uint64_t));  \
												__readSetSize[i]  = (uint64_t**)malloc(NUMBER_OF_TRANSACTIONS*sizeof(uint64_t*));  \
												__writeSetSize[i] = (uint64_t**)malloc(NUMBER_OF_TRANSACTIONS*sizeof(uint64_t*));  \
												int j;                                                                     \
												for(j=0; j < NUMBER_OF_TRANSACTIONS; j++){                                 \
													__readSetSize[i][j]  = (uint64_t*)calloc(MAX_TRANSACTIONS,sizeof(uint64_t));     \
													__writeSetSize[i][j] = (uint64_t*)calloc(MAX_TRANSACTIONS,sizeof(uint64_t));     \
												}                                                                          \
											}                                                                            \
										}

#define TM_SHUTDOWN() STM_SHUTDOWN();                                  \
					{ uint64_t i;                                                \
						uint64_t __numThreads__  = thread_getNumThread();          \
						for(i=0; i < __numThreads__; i++) {                        \
							uint64_t j;                                              \
							printf("Thread %ld\n",i);                                \
					  	for(j=0; j < NUMBER_OF_TRANSACTIONS; j++) {              \
								printf("Tx %2lu\n", j);                                \
								uint64_t k;                                            \
								for(k=0; k < __counter[i][j]; k++)                     \
									printf("%lu %lu\n", __readSetSize[i][j][k]           \
										,__writeSetSize[i][j][k]);                         \
								free(__readSetSize[i][j]); free(__writeSetSize[i][j]); \
							}                                                        \
							free(__counter[i]);                                      \
							free(__stm_commits[i]);  free(__stm_aborts[i]);          \
							free(__readSetSize[i]); free(__writeSetSize[i]);         \
						}                                                          \
						free(__counter);                                           \
						free(__stm_commits);  free(__stm_aborts);                  \
						free(__readSetSize); free(__writeSetSize);                 \
					}                                                            \
					pmuShutdown();                                               \
					msrTerminate()

#endif /* RW_SET_PROFILING */

#define TM_THREAD_ENTER()    long __threadId__ = thread_getId(); \
														 TM_ARGDECL_ALONE = STM_NEW_THREAD(); \
                             STM_INIT_THREAD(TM_ARG_ALONE, thread_getId()); \
														 set_affinity(__threadId__); \
														 norecInitThreadCommits(__stm_commits[__threadId__]); \
														 norecInitThreadAborts(__stm_aborts[__threadId__])

#define TM_THREAD_EXIT()     STM_FREE_THREAD(TM_ARG_ALONE)

#define TM_BEGIN()                    __txId__ = __COUNTER__;                   \
																			pmuStartCounting(__threadId__, __txId__); \
																			STM_BEGIN_WR()
#define TM_BEGIN_RO()                 __txId__ = __COUNTER__;                   \
																			pmuStartCounting(__threadId__, __txId__); \
																			STM_BEGIN_RD()
#define TM_END()                      STM_END();                    \
																			pmuStopCounting(__threadId__)

#elif defined(THROUGHPUT_PROFILING)

#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
#define __CACHE_ALIGNMENT__ 0x10000
#define CACHE_LINE_SIZE 128
#endif

#if defined(__x86_64__) || defined(__i386)
#define __CACHE_ALIGNMENT__ 0x1000
#define CACHE_LINE_SIZE  64
#endif

typedef struct throughputProfilingData_ {
	uint64_t sampleCount;
	uint64_t maxSamples;
	uint64_t stepCount;
	uint64_t sampleStep;	
	uint64_t before;
	double*  samples;
  char padding[CACHE_LINE_SIZE];
} throughputProfilingData_t;

#if defined(GENOME)
#define INIT_SAMPLE_STEP 1000 
#elif defined(INTRUDER)
#define INIT_SAMPLE_STEP 5000
#elif defined(KMEANS)
#define INIT_SAMPLE_STEP 2000
#elif defined(LABYRINTH)
#define INIT_SAMPLE_STEP 5
#elif defined(SSCA2)
#define INIT_SAMPLE_STEP 5000
#elif defined(VACATION)
#define INIT_SAMPLE_STEP 2000
#elif defined(YADA)
#define INIT_SAMPLE_STEP 1000
#else
#error "unknown application!"
#endif

#define INIT_MAX_SAMPLES 1000000

extern throughputProfilingData_t *__throughputProfilingData;

static inline uint64_t getTime() __attribute__((always_inline));
static inline uint64_t getTime() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint64_t)(t.tv_sec*1.0e6) + (uint64_t)(t.tv_nsec*1.0e-3);
}

extern void increaseThroughputSamplesSize(double **ptr, uint64_t *oldLength, uint64_t newLength);


#define TM_STARTUP(numThread)	msrInitialize();                    \
															STM_STARTUP(numThread);             \
															{ \
																__throughputProfilingData = (throughputProfilingData_t*)calloc(numThread, \
																	sizeof(throughputProfilingData_t)); \
																uint64_t i; \
																for (i=0; i < numThread; i++) { \
																	__throughputProfilingData[i].sampleStep = INIT_SAMPLE_STEP; \
																	__throughputProfilingData[i].maxSamples = INIT_MAX_SAMPLES; \
																	int r = posix_memalign((void**)&(__throughputProfilingData[i].samples),\
																		__CACHE_ALIGNMENT__, __throughputProfilingData[i].maxSamples*sizeof(double)); \
																	if ( r ) { \
																		fprintf(stderr, "error: failed to allocate samples array!\n"); \
																		exit(EXIT_FAILURE); \
																	} \
																} \
															}

#define TM_SHUTDOWN()	STM_SHUTDOWN(); \
											msrTerminate(); \
											{ \
												uint64_t nb_threads = (uint64_t)thread_getNumThread(); \
												uint64_t maxSamples = 0; \
												uint64_t i; \
												for (i = 0; i < nb_threads; i++) { \
													if (__throughputProfilingData[i].maxSamples > maxSamples) { \
														maxSamples = __throughputProfilingData[i].maxSamples; \
													} \
												} \
												double *samples = (double*)calloc(sizeof(double), maxSamples); \
												uint64_t nSamples = 0; \
												for (i = 0; i < nb_threads; i++) { \
													uint64_t j; \
													uint64_t n = __throughputProfilingData[i].sampleCount; \
													for (j=0; j < n; j++) { \
														samples[j] += __throughputProfilingData[i].samples[j]; \
													} \
													if (n > nSamples) nSamples = n; \
													free(__throughputProfilingData[i].samples); \
												} \
												free(__throughputProfilingData); \
												FILE* outfile = fopen("transactions.throughput", "w"); \
												for (i = 0; i < nSamples; i++) { \
													fprintf(outfile, "%0.3lf\n", samples[i]); \
												} \
												free(samples); \
												fclose(outfile); \
											}

#define TM_THREAD_ENTER()    long __threadId__ = thread_getId(); \
														 TM_ARGDECL_ALONE = STM_NEW_THREAD(); \
                             STM_INIT_THREAD(TM_ARG_ALONE, thread_getId()); \
														 set_affinity(__threadId__); \
														 throughputProfilingData_t *__thProfData = &__throughputProfilingData[__threadId__]; \
														 __thProfData->before = getTime()

#define TM_THREAD_EXIT()     STM_FREE_THREAD(TM_ARG_ALONE); \
														 { \
															 uint64_t now = getTime(); \
															 if (__thProfData->stepCount) { \
																 double t = now - __thProfData->before; \
																 double th = (__thProfData->stepCount*1.0e6)/t; \
																 __thProfData->samples[__thProfData->sampleCount] = th; \
															 } \
														 }

#define TM_BEGIN()                    STM_BEGIN_WR()
#define TM_BEGIN_RO()                 STM_BEGIN_RD()
#define TM_END()                      STM_END(); \
																			{ \
																				__thProfData->stepCount++; \
																				if (__thProfData->stepCount == __thProfData->sampleStep) { \
																					uint64_t now = getTime(); \
																					double t = now - __thProfData->before; \
																					double th = (__thProfData->sampleStep*1.0e6)/t; \
																					__thProfData->samples[__thProfData->sampleCount] = th; \
																					__thProfData->sampleCount++; \
																					if ( __thProfData->sampleCount == __thProfData->maxSamples ) { \
																						increaseThroughputSamplesSize(&(__thProfData->samples), \
																							&(__thProfData->maxSamples), 2*__thProfData->maxSamples); \
																					} \
																					__thProfData->before = now; \
																					__thProfData->stepCount = 0; \
																				} \
																			}
#else /* NO PROFILING */

#define TM_STARTUP(numThread)	msrInitialize();                    \
															STM_STARTUP(numThread)

#define TM_SHUTDOWN()	        STM_SHUTDOWN(); \
											        msrTerminate()

#define TM_THREAD_ENTER()     long __threadId__ = thread_getId(); \
														  TM_ARGDECL_ALONE = STM_NEW_THREAD(); \
                              STM_INIT_THREAD(TM_ARG_ALONE, thread_getId()); \
														  set_affinity(__threadId__)

#define TM_THREAD_EXIT()      STM_FREE_THREAD(TM_ARG_ALONE)

#define TM_BEGIN()            STM_BEGIN_WR()
#define TM_BEGIN_RO()         STM_BEGIN_RD()
#define TM_END()              STM_END()

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

#ifdef MAIN_FUNCTION_FILE

#if defined(COMMIT_RATE_PROFILING) || defined(RW_SET_PROFILING)

uint64_t **__stm_commits;
uint64_t **__stm_aborts;

#endif /* defined(COMMIT_RATE_PROFILING) || defined(RW_SET_PROFILING) */

#if defined(THROUGHPUT_PROFILING)

throughputProfilingData_t *__throughputProfilingData = NULL;


void increaseThroughputSamplesSize(double **ptr, uint64_t *oldLength, uint64_t newLength) {
	double *newPtr;
	int r = posix_memalign((void**)&newPtr, __CACHE_ALIGNMENT__, newLength*sizeof(double));
	if ( r ) {
		perror("posix_memalign");
		fprintf(stderr, "error: increaseThroughputSamplesSize failed to increase throughputSamples array!\n");
		exit(EXIT_FAILURE);
	}
	memcpy((void*)newPtr, (const void*)*ptr, (*oldLength)*sizeof(double));
	free(*ptr);
	*ptr = newPtr;
	*oldLength = newLength;
}
#endif /* THROUGHPUT_PROFILING */

#endif /* MAIN_FUNCTION_FILE */
