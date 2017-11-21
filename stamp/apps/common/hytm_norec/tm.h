#ifndef _TM_H
#define _TM_H
/*
 * File:
 *   tm.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Empty file (to avoid source modifications red-black tree).
 *
 * Copyright (c) 2007-2012.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This program has a dual license and can also be distributed
 * under the terms of the MIT license.
 */

#define RO	1
#define RW	0

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
#define TM_FREE2(ptr,size)            TM_FREE(ptr)

#if defined(THROUGHPUT_PROFILING)

#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
#define __CACHE_ALIGNMENT__ 0x10000
#endif

#if defined(__x86_64__) || defined(__i386)
#define __CACHE_ALIGNMENT__ 0x1000
#endif

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

#define TM_STARTUP(nThreads)	        stm::sys_init(NULL); \
																			msrInitialize(); \
																			{ \
																				__throughputProfilingData = (throughputProfilingData_t*)calloc(nThreads, \
																					sizeof(throughputProfilingData_t)); \
																				uint64_t i; \
																				for (i=0; i < nThreads; i++) { \
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

#define TM_SHUTDOWN(nThreads)         stm::sys_shutdown(); \
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
																			throughputProfilingData_t *__thProfData = &__throughputProfilingData[__tid__]; \
																			__thProfData->before = getTime()

#define TM_THREAD_EXIT()              stm::thread_shutdown(); \
																			{ \
																				uint64_t now = getTime(); \
																				if (__thProfData->stepCount) { \
																					double t = now - __thProfData->before; \
																					double th = (__thProfData->stepCount*1.0e6)/t; \
																					__thProfData->samples[__thProfData->sampleCount] = th; \
																				} \
																			}

#define STM_START(ro, abort_flags)            \
    stm::TxThread* tx = (stm::TxThread*)stm::Self; \
    stm::begin(tx, &_jmpbuf, abort_flags);         \
    CFENCE; 

#define STM_COMMIT   stm::commit(tx)

#define IF_HTM_MODE							do { \
																	if ( HyTM::HTM_Begin_Tx() ) {
#define START_HTM_MODE            	CFENCE;
#define COMMIT_HTM_MODE							HyTM::HTM_Commit_Tx();
#define ELSE_STM_MODE							} else {
#define START_STM_MODE(ro)					jmp_buf _jmpbuf; \
																		uint32_t abort_flags = setjmp(_jmpbuf); \
																		STM_START(ro, abort_flags);
#define COMMIT_STM_MODE							STM_COMMIT; \
																	} \
																} while(0); \
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

#define TM_STARTUP(nThreads)	        stm::sys_init(NULL); \
																			msrInitialize()

#define TM_SHUTDOWN(nThreads)         stm::sys_shutdown(); \
																			msrTerminate()

#define TM_THREAD_ENTER()             long __tid__ = thread_getId(); \
																			set_affinity(__tid__);   \
																			stm::thread_init() 

#define TM_THREAD_EXIT()              stm::thread_shutdown()

#define STM_START(ro, abort_flags)            \
    stm::TxThread* tx = (stm::TxThread*)stm::Self; \
    stm::begin(tx, &_jmpbuf, abort_flags);         \
    CFENCE; 

#define STM_COMMIT   stm::commit(tx)

#define IF_HTM_MODE							do { \
																	if ( HyTM::HTM_Begin_Tx() ) {
#define START_HTM_MODE            	CFENCE;
#define COMMIT_HTM_MODE							HyTM::HTM_Commit_Tx();
#define ELSE_STM_MODE							} else {
#define START_STM_MODE(ro)					jmp_buf _jmpbuf; \
																		uint32_t abort_flags = setjmp(_jmpbuf); \
																		STM_START(ro, abort_flags);
#define COMMIT_STM_MODE							STM_COMMIT; \
																	} \
																} while(0);
#endif /* NO PROFILING */

#define TM_RESTART()                  stm::restart()

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

#endif /* _TM_H */

#ifdef MAIN_FUNCTION_FILE

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
