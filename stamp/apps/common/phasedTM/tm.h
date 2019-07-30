#ifndef _TM_H
#define _TM_H

#include <phTM.h>

#if defined(__x86_64__) || defined(__i386)
#include <msr.h>
#include <pmu.h>
#else
#define msrInitialize()         		/* nothing */
#define msrTerminate()          		/* nothing */
#endif

#define RO	1
#define RW	0

#define TinySTM 1
#define NOrec   2

#define MAIN(argc, argv)            int main (int argc, char** argv)
#define MAIN_RETURN(val)            return val

#define GOTO_SIM()                  /* nothing */
#define GOTO_REAL()                 /* nothing */
#define IS_IN_SIM()                 (0)

#define SIM_GET_NUM_CPU(var)        /* nothing */

#define P_MEMORY_STARTUP(numThread) /* nothing */
#define P_MEMORY_SHUTDOWN()         /* nothing */

#if STM == TinySTM

#include <stm.h>
#include <mod_mem.h>
#include <mod_ab.h>
#include <mod_stats.h>

#define TM_ARG                      /* nothing */
#define TM_ARG_ALONE                /* nothing */
#define TM_ARGDECL                  /* nothing */
#define TM_ARGDECL_ALONE            /* nothing */
#define TM_SAFE                     /* nothing */
#define TM_PURE                     /* nothing */

#define SEQ_MALLOC(size)            malloc(size)
#define SEQ_FREE(ptr)               free(ptr)
#define P_MALLOC(size)              malloc(size)
#define P_FREE(ptr)                 free(ptr)
#define TM_MALLOC(size)             stm_malloc(size)
/* Note that we only lock the first word and not the complete object */
#define TM_FREE(ptr)                stm_free(ptr, sizeof(stm_word_t))

/*
 * Useful macros to work with transactions. Note that, to use nested
 * transactions, one should check the environment returned by
 * stm_get_env() and only call sigsetjmp() if it is not null.
 */
#if STM_VERSION_NB <= 103
# define STM_START(ro, tx_count, restarted)     \
																		do { \
                                      sigjmp_buf *_e; \
                                      stm_tx_attr_t _a = {0, ro}; \
                                      _e = stm_start(&_a, tx_count); \
                                      int status = sigsetjmp(*_e, 0); \
																			(*restarted) = status != 0; \
                                    } while (0)
#else /* STM_VERSION_NB > 103 */
# define STM_START(ro, tx_count, restarted)			\
																		do { \
                                      sigjmp_buf *_e; \
                                      stm_tx_attr_t _a = {{.id = 0, .read_only = ro, .visible_reads = 0}}; \
                                      _e = stm_start(_a, tx_count); \
                                      int status = sigsetjmp(*_e, 0); \
																			(*restarted) = status != 0; \
                                    } while (0)
#endif /* STM_VERSION_NB > 103 */

#define STM_COMMIT                  stm_commit()

#define IF_HTM_MODE							while(1){ \
																	uint64_t mode = getMode(); \
																	if (mode == HW || mode == GLOCK){
#define START_HTM_MODE 							bool modeChanged = HTM_Start_Tx(); \
																		if (!modeChanged) {
#define COMMIT_HTM_MODE								HTM_Commit_Tx(); \
																			break; \
																		}
#define ELSE_STM_MODE							} else {
#define START_STM_MODE(ro)					bool restarted = false;                        \
																		STM_START(ro, __COUNTER__, &restarted);        \
																		bool modeChanged = STM_PreStart_Tx(restarted); \
																		if (!modeChanged){
#define COMMIT_STM_MODE								STM_COMMIT;                                  \
																			STM_PostCommit_Tx();                         \
																			break;                                       \
																		}                                              \
																		STM_COMMIT;                                    \
																	}                                                \
																}

#define TM_RESTART()            { \
																	uint64_t mode = getMode(); \
																	if(mode == SW) stm_abort(0); \
																	else htm_abort(); \
																}
#define TM_EARLY_RELEASE(var)       /* nothing */

#include <wrappers.h>

/* We could also map macros to the stm_(load|store)_long functions if needed */

typedef union { stm_word_t w; float f;} floatconv_t;

#define TM_SHARED_READ(var)           stm_load((volatile stm_word_t *)(void *)&(var))
#define TM_SHARED_READ_P(var)         stm_load_ptr((volatile void **)(void *)&(var))
#define TM_SHARED_READ_F(var)         stm_load_float((volatile float *)(void *)&(var))
//#define TM_SHARED_READ_P(var)         stm_load((volatile stm_word_t *)(void *)&(var))
//#define TM_SHARED_READ_F(var)         ({floatconv_t c; c.w = stm_load((volatile stm_word_t *)&(var)); c.f;})

#define TM_SHARED_WRITE(var, val)     stm_store((volatile stm_word_t *)(void *)&(var), (stm_word_t)val)
#define TM_SHARED_WRITE_P(var, val)   stm_store_ptr((volatile void **)(void *)&(var), val)
#define TM_SHARED_WRITE_F(var, val)   stm_store_float((volatile float *)(void *)&(var), val)
//#define TM_SHARED_WRITE_P(var, val)   stm_store((volatile stm_word_t *)(void *)&(var), (stm_word_t)val)
//#define TM_SHARED_WRITE_F(var, val)   ({floatconv_t c; c.f = val; stm_store((volatile stm_word_t *)&(var), c.w);})

/* TODO: test with mod_log */
#define TM_LOCAL_WRITE(var, val)      var = val
#define TM_LOCAL_WRITE_P(var, val)    var = val
#define TM_LOCAL_WRITE_F(var, val)    var = val

#define TM_IFUNC_DECL
#define TM_IFUNC_CALL1(r, f, a1)      r = f(a1)
#define TM_IFUNC_CALL2(r, f, a1, a2)  r = f((a1), (a2))

#define TM_STARTUP(nThreads)	msrInitialize(); \
															stm_init();      \
															mod_mem_init(0); \
                              phTM_init()

#define TM_SHUTDOWN()       phTM_term(); \
														stm_exit(); \
														msrTerminate()

#define TM_THREAD_ENTER()           long __tid__ = thread_getId(); \
																		set_affinity(__tid__);                       \
																		stm_init_thread(NUMBER_OF_TRANSACTIONS); \
																		phTM_thread_init()

#define TM_THREAD_EXIT()    stm_exit_thread(); \
														phTM_thread_exit(0, 0)

#elif STM == NOrec

#include <stdio.h>
#include <string.h>
#include <api/api.hpp>
#include <stm/txthread.hpp>
#include <thread.h>

#undef TM_ARG
#undef TM_ARG_ALONE 

#define TM_SAFE                       /* nothing */
#define TM_PURE                       /* nothing */
#define TM_CALLABLE                   /* nothing */

#define TM_ARG                        /* nothing */
#define TM_ARG_ALONE                  /* nothing */
#define TM_ARGDECL                    /* nothing */
#define TM_ARGDECL_ALONE              /* nothing */

#define P_MALLOC(size)                malloc(size)
#define P_FREE(ptr)                   free(ptr)
#define SEQ_MALLOC(size)              malloc(size)
#define SEQ_FREE(ptr)                 free(ptr)
#define TM_MALLOC(size)               TM_ALLOC(size)
/* TM_FREE(ptr) is already defined in the file interface. */

#if defined(HTM_STATUS_PROFILING)
extern __thread long __txId__;
extern __thread uint64_t __stm_commits;
extern __thread uint64_t __stm_aborts;

extern void norecInitThreadCommits(uint64_t* addr);
extern void norecInitThreadAborts(uint64_t* addr);

#define ALLOCA_COMMITS_ABORTS_VARIABLES(numThread) /* nothing */
#define INIT_COMMITS_ABORTS_VARIABLES() \
	norecInitThreadCommits(&__stm_commits); \
	norecInitThreadAborts(&__stm_aborts)

#define NUM_COMMITS __stm_commits
#define NUM_ABORTS __stm_aborts

#else

#define NUM_COMMITS 0
#define NUM_ABORTS  0
#define ALLOCA_COMMITS_ABORTS_VARIABLES(numThread) /* nothing */
#define INIT_COMMITS_ABORTS_VARIABLES() /* nothing */ 

#endif

#if defined(THROUGHPUT_PROFILING)

typedef struct throughputProfilingData_ {
	uint64_t sampleCount;
	uint64_t maxSamples;
	uint64_t stepCount;
	uint64_t sampleStep;	
	uint64_t before;
	double*  samples;
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

extern inline uint64_t getTime(){
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint64_t)(t.tv_sec*1.0e6) + (uint64_t)(t.tv_nsec*1.0e-3);
}

extern void increaseThroughputSamplesSize(double **ptr, uint64_t *oldLength, uint64_t newLength);

#define TM_STARTUP(numThread)					msrInitialize();        \
																			stm::sys_init(NULL); \
																			phTM_init(); \
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

#define TM_SHUTDOWN()                 phTM_term(); \
																			stm::sys_shutdown(); \
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

#define TM_THREAD_ENTER()             long __tid__ = thread_getId(); \
																			set_affinity(__tid__);   \
																			stm::thread_init(); \
																			phTM_thread_init(); \
																			throughputProfilingData_t *__thProfData = &__throughputProfilingData[__tid__]; \
																			__thProfData->before = getTime()

#define TM_THREAD_EXIT()              stm::thread_shutdown(); \
																			phTM_thread_exit(NUM_COMMITS, NUM_ABORTS); \
																			{ \
																				uint64_t now = getTime(); \
																				if (__thProfData->stepCount) { \
																					double t = now - __thProfData->before; \
																					double th = (__thProfData->stepCount*1.0e6)/t; \
																					__thProfData->samples[__thProfData->sampleCount] = th; \
																				} \
																			}

#define STM_START(abort_flags)            				 \
    stm::TxThread* tx = (stm::TxThread*)stm::Self; \
    stm::begin(tx, &_jmpbuf, abort_flags);         \
    CFENCE; 

#define STM_COMMIT  stm::commit(tx)

#define IF_HTM_MODE							while(1){ \
																	uint64_t mode = getMode(); \
																	if (mode == HW || mode == GLOCK){
#define START_HTM_MODE 							bool modeChanged = HTM_Start_Tx(); \
																		if (!modeChanged) {
#define COMMIT_HTM_MODE								HTM_Commit_Tx(); \
																			break; \
																		}
#define ELSE_STM_MODE							} else {
#define START_STM_MODE(ro)			    jmp_buf _jmpbuf; \
																		uint32_t abort_flags = setjmp(_jmpbuf); \
																		bool restarted = abort_flags != 0; \
																		bool modeChanged = STM_PreStart_Tx(restarted); \
																		if (!modeChanged){ \
																			STM_START(abort_flags);
#define COMMIT_STM_MODE								STM_COMMIT; \
																			STM_PostCommit_Tx(); \
																			break; \
																		} \
																	} \
																} \
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

#define TM_STARTUP(numThread)					msrInitialize();        \
																			stm::sys_init(NULL); \
																			ALLOCA_COMMITS_ABORTS_VARIABLES(numThread); \
																			phTM_init()

#define TM_SHUTDOWN()                 phTM_term(); \
																			stm::sys_shutdown(); \
											                msrTerminate()

#define TM_THREAD_ENTER()             long __tid__ = thread_getId(); \
																			set_affinity(__tid__);   \
																			stm::thread_init(); \
																			INIT_COMMITS_ABORTS_VARIABLES(); \
																			phTM_thread_init()

#define TM_THREAD_EXIT()              stm::thread_shutdown(); \
																			phTM_thread_exit(NUM_COMMITS, NUM_ABORTS)

#define STM_START(abort_flags)            				 \
    stm::TxThread* tx = (stm::TxThread*)stm::Self; \
    stm::begin(tx, &_jmpbuf, abort_flags);         \
    CFENCE; 

#define STM_COMMIT  stm::commit(tx)

#define IF_HTM_MODE							while(1){ \
																	uint64_t mode = getMode(); \
																	if (mode == HW || mode == GLOCK){
#define START_HTM_MODE 							bool modeChanged = HTM_Start_Tx(); \
																		if (!modeChanged) {
#define COMMIT_HTM_MODE								HTM_Commit_Tx(); \
																			break; \
																		}
#define ELSE_STM_MODE							} else {
#define START_STM_MODE(ro)			    jmp_buf _jmpbuf; \
																		uint32_t abort_flags = setjmp(_jmpbuf); \
																		bool restarted = abort_flags != 0; \
																		bool modeChanged = STM_PreStart_Tx(restarted); \
																		if (!modeChanged){ \
																			STM_START(abort_flags);
#define COMMIT_STM_MODE								STM_COMMIT; \
																			STM_PostCommit_Tx(); \
																			break; \
																		} \
																	} \
																}
#endif /* NO PROFILING */															

#define TM_RESTART()            { \
																	uint64_t mode = getMode(); \
																	if(mode == SW) stm::restart(); \
																	else htm_abort(); \
																}
#define TM_EARLY_RELEASE(var)         /* nothing */

#define TM_LOAD(addr)                 stm::stm_read(addr, (stm::TxThread*)stm::Self)
#define TM_STORE(addr, val)           stm::stm_write(addr, val, (stm::TxThread*)stm::Self)

#define TM_SHARED_READ(var)           TM_LOAD(&var)
#define TM_SHARED_READ_P(var)         TM_LOAD(&var)
#define TM_SHARED_READ_F(var)         TM_LOAD(&var)

#define TM_SHARED_WRITE(var, val)     TM_STORE(&var, val)
#define TM_SHARED_WRITE_P(var, val)   TM_STORE(&var, val)
#define TM_SHARED_WRITE_F(var, val)   TM_STORE(&var, val)

#define TM_LOCAL_WRITE(var, val)     	({var = val; var;})
#define TM_LOCAL_WRITE_P(var, val)    ({var = val; var;})
#define TM_LOCAL_WRITE_F(var, val)    ({var = val; var;})
#define TM_LOCAL_WRITE_D(var, val)    ({var = val; var;})

#define TM_IFUNC_DECL                 /* nothing */
#define TM_IFUNC_CALL1(r, f, a1)      r = f(a1)
#define TM_IFUNC_CALL2(r, f, a1, a2)  r = f((a1), (a2))

#else
#error "no STM selected!"
#endif

#endif /* _TM_H */

#ifdef MAIN_FUNCTION_FILE

#if defined(HTM_STATUS_PROFILING)
__thread uint64_t __stm_commits;
__thread uint64_t __stm_aborts;
#endif

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
