/*
 * File:
 *   intset.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Integer set stress test.
 *
 * Copyright (c) 2007-2013.
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

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <malloc.h> // memalign

#include <tm.h>

#ifdef DEBUG
# define IO_FLUSH                       fflush(NULL)
/* Note: stdio is thread-safe */
#endif

#include <common.h>
#include <linkedlist.h>
#include <skiplist.h>
#include <hashset.h>
#include <rbtree.h>

#define DEFAULT_NB_PHASES               1
#define DEFAULT_SET_IMPL                LL

#define DEFAULT_DURATION                1000
#define DEFAULT_INITIAL                 256
#define DEFAULT_NB_THREADS              1
#define DEFAULT_RANGE                   (DEFAULT_INITIAL * 2)
#define DEFAULT_SEED                    0
#define DEFAULT_UPDATE                  20
#define DEFAULT_ALTERNATE               1

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

#define TARGET_NUM_SAMPLES 1000
#define INIT_MAX_SAMPLES (2*TARGET_NUM_SAMPLES)

#include <unistd.h>
#include <sched.h>
void set_affinity(long id){
	int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
	if (id < 0 || id >= num_cores){
		fprintf(stderr,"error: invalid number of threads (nthreads > ncores)!\n");
		exit(EXIT_FAILURE);
	}
	
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
	int hw_tid = (id%4)*8 + id/4;
  // 4 cores, 8 threads per core
	/* core | hw_thread
	 *  0   |    0..7
	 *  1   |   8..15
	 *  2   |  16..23
	 *  3   |  24..31 */
	CPU_SET(hw_tid, &cpuset);
#else /* Haswell */
	int hw_tid = id;
  // 4 cores, 2 threads per core
	/* core | hw_thread
	 *  0   |   0,4
	 *  1   |   1,5
	 *  2   |   2,6
	 *  3   |   3,7 */
	CPU_SET(hw_tid, &cpuset);
#endif /* Haswell*/

	pthread_t current_thread = pthread_self();
	if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset)){
		perror("pthread_setaffinity_np");
		exit(EXIT_FAILURE);
	}

	while( hw_tid != sched_getcpu() );
}

#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
static inline uint64_t getCycles()
{
		uint32_t upper, lower,tmp;
		__asm__ volatile(
			"0:                  \n"
			"\tmftbu   %0        \n"
			"\tmftb    %1        \n"
			"\tmftbu   %2        \n"
			"\tcmpw    %2,%0     \n"
			"\tbne     0b        \n"
   	 : "=r"(upper),"=r"(lower),"=r"(tmp)
	  );
		return  (((uint64_t)upper) << 32) | lower;
}
#elif defined(__x86_64__)
static inline uint64_t getCycles()
{
    uint32_t tmp[2];
    __asm__ ("rdtsc" : "=a" (tmp[1]), "=d" (tmp[0]) : "c" (0x10) );
    return (((uint64_t)tmp[0]) << 32) | tmp[1];
}
#else
#error "unsupported architecture!"
#endif

/* ################################################################### *
 * GLOBALS
 * ################################################################### */
static volatile int stop;
unsigned short main_seed[3];

/* ################################################################### *
 * BARRIER
 * ################################################################### */

typedef struct barrier {
  pthread_cond_t complete;
  pthread_mutex_t mutex;
  int count;
  int crossing;
} barrier_t;

static void barrier_init(barrier_t *b, int n)
{
  pthread_cond_init(&b->complete, NULL);
  pthread_mutex_init(&b->mutex, NULL);
  b->count = n;
  b->crossing = 0;
}

static void barrier_cross(barrier_t *b)
{
  pthread_mutex_lock(&b->mutex);
  /* One more thread through */
  b->crossing++;
  /* If not all here, wait */
  if (b->crossing < b->count) {
    pthread_cond_wait(&b->complete, &b->mutex);
  } else {
    pthread_cond_broadcast(&b->complete);
    /* Reset for next time */
    b->crossing = 0;
		stop = 0;
  }
  pthread_mutex_unlock(&b->mutex);
}

#ifdef THROUGHPUT_PROFILING

inline static
uint64_t getTime(){
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint64_t)(t.tv_sec*1.0e9) + (uint64_t)(t.tv_nsec);
}

inline static
void increaseThroughputSamplesSize(double **ptr, uint64_t *oldLength, uint64_t newLength) {
	double *newPtr;
	int r = posix_memalign((void**)&newPtr, __CACHE_ALIGNMENT__, newLength*sizeof(double));
	if ( r ) {
		perror("posix_memalign");
		fprintf(stderr, "error: failed to increase throughputSamples array!\n");
		exit(EXIT_FAILURE);
	}
	memcpy((void*)newPtr, (const void*)*ptr, (*oldLength)*sizeof(double));
	free(*ptr);
	*ptr = newPtr;
	*oldLength = newLength;
}

#define THROUGHPUT_PROFILING_VARS \
		uint64_t stepCount; \
		stepCount = 0; \
		uint64_t sample_step; \
		sample_step = TARGET_NUM_SAMPLES; \
		uint64_t before; \
		before = getTime(); \
		double *throughputSamples = p->throughputSamples; 

#define THROUGHPUT_PROFILING_CODE \
			if (stepCount == sample_step) { \
				uint64_t now = getTime(); \
				double t = now - before; \
				double th = (sample_step*1.0e9)/t; \
				throughputSamples[p->sampleCount] = th; \
				sample_step = ((uint64_t)th)/TARGET_NUM_SAMPLES; \
				p->sampleCount++; \
				if ( p->sampleCount == p->maxSamples ) { \
					increaseThroughputSamplesSize(&(p->throughputSamples), &(p->maxSamples), 2*p->maxSamples); \
					throughputSamples = p->throughputSamples; \
				} \
				before = now; \
				stepCount = 0; \
			} else { \
				stepCount++; \
			} 

#else

#define THROUGHPUT_PROFILING_VARS /* nothing */
#define THROUGHPUT_PROFILING_CODE /* nothing */

#endif /* ! THROUGHPUT_PROFILING */

/* ################################################################### *
 * STRESS TEST
 * ################################################################### */

#define STRESS_TEST(SET_NAME, SET_TYPE, SET_PREFIX) \
	static void SET_NAME##_test(thread_data_t *d, phase_data_t *p, SET_TYPE *set_ptr) \
	{ \
		THROUGHPUT_PROFILING_VARS \
		int op, val, last = -1; \
	  while (stop == 0) { \
	    op = rand_range(100, d->seed); \
	    if (op < p->update) { \
	      if (p->alternate) { \
	        /* Alternate insertions and removals */ \
	        if (last < 0) { \
	          /* Add random value */ \
	          val = rand_range(p->range, d->seed) + 1; \
	          if (SET_NAME##_add(set_ptr, val, d)) { \
	            p->diff++; \
	            last = val; \
	          } \
	          p->nb_add++; \
	        } else { \
	          /* Remove last value */ \
	          if (SET_NAME##_remove(set_ptr, last, d)) \
	            p->diff--; \
	          p->nb_remove++; \
	          last = -1; \
	        } \
	      } else { \
	        /* Randomly perform insertions and removals */ \
	        val = rand_range(p->range, d->seed) + 1; \
	        if ((op & 0x01) == 0) { \
	          /* Add random value */ \
	          if (SET_NAME##_add(set_ptr, val, d)) \
	            p->diff++; \
	          p->nb_add++; \
	        } else { \
	          /* Remove random value */ \
	          if (SET_NAME##_remove(set_ptr, val, d)) \
	            p->diff--; \
	          p->nb_remove++; \
	        } \
	      } \
	    } else { \
	      /* Look for random value */ \
	      val = rand_range(p->range, d->seed) + 1; \
	      if (SET_NAME##_contains(set_ptr, val, d)) \
	        p->nb_found++; \
	      p->nb_contains++; \
	    } \
			THROUGHPUT_PROFILING_CODE \
	  } \
	} \

STRESS_TEST(llistset, llistset_t, LL_)
STRESS_TEST(slistset, slistset_t, SL_)
STRESS_TEST(hashset, hashset_t, HS_)
STRESS_TEST(rbtreeset, rbtree_t, RB_)

static void *test(void *data)
{
  thread_data_t *d = (thread_data_t *)data;

  /* Create transaction */
  TM_INIT_THREAD(d->threadId);

	int i, nb_phases = d->nb_phases;
	for (i = 0; i < nb_phases; i++) {
		phase_data_t *p = &(d->phases[i]);
  	/* Wait on barrier */
		barrier_cross(d->barrier);
		switch(p->setImpl) {
			case LL:
				llistset_test(d, p, (llistset_t*)p->set_ptr);
				break;
			case HS:
				hashset_test(d, p, (hashset_t*)p->set_ptr);
				break;
			case RB:
				rbtreeset_test(d, p, (rbtree_t*)p->set_ptr);
				break;
		  case SL:
				slistset_test(d, p, (slistset_t*)p->set_ptr);
				break;
			default:
				fprintf(stderr,"error: invalid set type!\n");
				exit(-1);
				break;
		}
	}
  
	/* Free transaction */
  TM_EXIT_THREAD(d->threadId);

  return NULL;
}

#define INIT_SET(SET_NAME, SET_TYPE) \
	static void SET_NAME##_init(phase_data_t *p, SET_TYPE *set_ptr) \
	{ \
  	int i = 0, val; \
		int initial = p->initial; \
		int range = p->range; \
  	while (i < initial) { \
    	val = rand_range(range, main_seed) + 1; \
    	if (SET_NAME##_add(set_ptr, val, 0)) \
    	  i++; \
  	} \
	}

INIT_SET(llistset, llistset_t)
INIT_SET(slistset, slistset_t)
INIT_SET(hashset, hashset_t)
INIT_SET(rbtreeset, rbtree_t)

static void init_set(phase_data_t *p){
	switch(p->setImpl) {
		case LL:
			llistset_init(p, (llistset_t*)p->set_ptr);
			break;
		case HS:
			hashset_init(p, (hashset_t*)p->set_ptr);
			break;
		case RB:
			rbtreeset_init(p, (rbtree_t*)p->set_ptr);
			break;
	  case SL:
			slistset_init(p, (slistset_t*)p->set_ptr);
			break;
		default:
			fprintf(stderr,"error: invalid set type!\n");
			exit(-1);
			break;
	}
}

static void* set_new(set_impl_t setImpl) {
	switch(setImpl) {
		case LL:
			return (void*)llistset_new();
			break;
		case HS:
			return (void*)hashset_new();
			break;
		case RB:
			return (void*)rbtreeset_new();
			break;
	  case SL:
			return (void*)slistset_new();
			break;
		default:
			fprintf(stderr,"error: invalid set type!\n");
			exit(-1);
			break;
	}
}

static void set_delete(phase_data_t *p) {
	switch(p->setImpl) {
		case LL:
			llistset_delete((llistset_t*)p->set_ptr);
			break;
		case HS:
			hashset_delete((hashset_t*)p->set_ptr);
			break;
		case RB:
			rbtreeset_delete((rbtree_t*)p->set_ptr);
			break;
	  case SL:
			slistset_delete((slistset_t*)p->set_ptr);
			break;
		default:
			fprintf(stderr,"error: invalid set type!\n");
			exit(-1);
			break;
	}
}

static int set_size(phase_data_t* p) {
	switch(p->setImpl) {
		case LL:
			return llistset_size((llistset_t*)p->set_ptr);
			break;
		case HS:
			return hashset_size((hashset_t*)p->set_ptr);
			break;
		case RB:
			return rbtreeset_size((rbtree_t*)p->set_ptr);
			break;
	  case SL:
			return slistset_size((slistset_t*)p->set_ptr);
			break;
		default:
			fprintf(stderr,"error: invalid set type!\n");
			exit(-1);
			break;
	}
}

static void init_phase_data(phase_data_t *p, int nb_phases){
	
	int i;
	for (i = 0; i < nb_phases; i++) {
		p[i].setImpl = LL;
		p[i].initial = DEFAULT_INITIAL;
		p[i].update = DEFAULT_UPDATE;
		p[i].duration = DEFAULT_DURATION;
		p[i].range = 2*p[i].initial;
		p[i].alternate = DEFAULT_ALTERNATE;
	}
}

static phase_data_t *copy_phases(phase_data_t* phases, int nb_phases){
	
	phase_data_t *phases_copy = (phase_data_t*)malloc(nb_phases * sizeof(phase_data_t));
	int i;
	for (i = 0; i < nb_phases; i++) {
		memcpy(&phases_copy[i], &phases[i], sizeof(phase_data_t));
	}
	return phases_copy;
}

int main(int argc, char **argv)
{
  struct option long_options[] = {
    // These options don't set a flag
    {"help",                      no_argument,       NULL, 'h'},
    {"num-threads",               required_argument, NULL, 'n'},
    {"number-of-phases",          required_argument, NULL, 'm'},
    {"phase-config",              required_argument, NULL, 'p'},
    {"seed",                      required_argument, NULL, 's'},
    {NULL, 0, NULL, 0}
  };

	//int size;
  int i, c, ret = 0;
  thread_data_t *data;
  pthread_t *threads;
  pthread_attr_t attr;
  barrier_t barrier;
  struct timeval start, end;
  struct timespec timeout;

  int nb_threads = DEFAULT_NB_THREADS;
  int seed = DEFAULT_SEED;
	
	int nb_phases  = DEFAULT_NB_PHASES;
	phase_data_t *phases = NULL;
  
	sigset_t block_set;

#ifdef THROUGHPUT_PROFILING
	FILE *outfile = fopen("transactions.throughput", "w");
	if ( outfile == NULL ) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}
#endif /* THROUGHPUT_PROFILING */

	int j = 0;
	char *buffer;
  while(1) {
  	i = 0;
    c = getopt_long(argc, argv, "h"
                    "n:m:p:d:s:"
                    , long_options, &i);

    if(c == -1)
      break;

    if(c == 0 && long_options[i].flag == 0)
      c = long_options[i].val;

    switch(c) {
    	case 0:
      	/* Flag is automatically set */
      	break;
    	case 'h':
      	printf("pintset -- STM stress test "
              "\n"
              "Usage:\n"
              "  pintset [options...]\n"
              "\n"
              "Options:\n"
              "  -h, --help\n"
              "        Print this message\n"
              "  -t, --num-threads <int>\n"
              "        Number of threads (default=" XSTR(DEFAULT_NB_THREADS) ")\n"
              "  -n, --number-of-phases <int>\n"
              "        percentage of update transactions (default=" XSTR(DEFAULT_NB_PHASES) ")\n"
              "  -p, --phase-config <string>\n"
              "        phase configuration string, ex: 'LL:4096:0:1000' (default=NONE)\n"
              "  -s, --seed <int>\n"
              "        RNG seed (0=time-based, default=" XSTR(DEFAULT_SEED) ")\n"
        );
      	exit(0);
			case 'n':
      	nb_threads = atoi(optarg);
      	break;
			case 'm':
      	nb_phases = atoi(optarg);
				if ( nb_phases < 1 ) {
					fprintf(stderr,"error: invalid number of phases (must be at least 1)!\n");
					exit(EXIT_FAILURE);
				}
				phases = (phase_data_t*)calloc(nb_phases, sizeof(phase_data_t));
				init_phase_data(phases, nb_phases);
      	break;
			case 'p':
				if ( nb_phases < 1 ) {
					fprintf(stderr,"error: nb_phases not specified or spedicified after first phase-config!\n");
					exit(EXIT_FAILURE);
				}
				if ( phases == NULL ) {
					phases = (phase_data_t*)calloc(nb_phases, sizeof(phase_data_t));
					init_phase_data(phases, nb_phases);
				}
				if ( j < nb_phases ) {
					// 1st token is the set implementation
					buffer = strtok(optarg, ":");
					if ( buffer != NULL ) {
						if ( strncmp(buffer, "LL", 2) == 0 ) {
							// linked-list
							phases[j].setImpl = LL;
						} else if ( strncmp(buffer, "HS", 2) == 0 ) {
							// hash-set
							phases[j].setImpl = HS;
						} else if ( strncmp(buffer, "RB", 2) == 0 ) {
							// red-black tree
							phases[j].setImpl = RB;
						} else if ( strncmp(buffer, "SL", 2) == 0 ) {
							// skip-list
							phases[j].setImpl = SL;
						} else {
							fprintf(stderr,"error: invalid set implementations! possible values are: LL, RB, HS or SL.\n");
						}
					}
					// 2nd token is initial set size
					buffer = strtok(NULL, ":");
					if ( buffer != 0 ) phases[j].initial = atoi(buffer);
					// 3rd token is update-rate
					buffer = strtok(NULL, ":");
					if ( buffer != 0 ) phases[j].update = atoi(buffer);
					// 4rd token is duration
					buffer = strtok(NULL, ":");
					if ( buffer != 0 ) phases[j].duration = atoi(buffer);
					// setting range as twice the initial size
					phases[j].range = 2*phases[j].initial;
					// using default for the remaining parameters
					phases[j].alternate = DEFAULT_ALTERNATE;
					j++;
				} else {
					fprintf(stderr,"error: nb_phases is smaller than the number of phase-config specifications!\n");
					exit(EXIT_FAILURE);
				}
				break;
			case 's':
				seed = atoi(optarg);
				break;
			case '?':
      	printf("Use -h or --help for help\n");
      	exit(0);
			default:
				exit(1);
    }
  }
	// if no arguments were given, then init with default values
	if ( phases == NULL ) {
		phases = (phase_data_t*)calloc(nb_phases, sizeof(phase_data_t));
		init_phase_data(phases, nb_phases);
	}

  if (seed == 0)
		srand((int)time(NULL));
  else
		srand(seed);

  /* Thread-local seed for main thread */
  rand_init(main_seed);

	printf("Nb threads         : %d\n", nb_threads);
	printf("Seed               : %d\n", seed);
	printf("--------------------------------\n");
	for (i = 0; i < nb_phases; i++) {
		printf("Phase #%d\n", i);
  	printf("Set Implementation : %s\n", set_impl_map[phases[i].setImpl]);
 		printf("Value range        : %d\n", phases[i].range);
		printf("Update rate        : %d\n", phases[i].update);
		printf("Alternate          : %d\n", phases[i].alternate);
  	printf("Duration           : %d\n", phases[i].duration);
  	/* Populate set */
  	printf("Adding %d entries to set\n", phases[i].initial);
		phases[i].set_ptr = set_new(phases[i].setImpl);
		init_set(&(phases[i]));
		phases[i].initial = set_size(&(phases[i]));
  	printf("Initial size       : %d\n", phases[i].initial);
  	if (phases[i].alternate == 0 && phases[i].range != phases[i].initial * 2)
    	printf("WARNING: range is not twice the initial set size\n");
		printf("--------------------------------\n");
	}
  printf("Type sizes         : int=%d/long=%d/ptr=%d/word=%d\n",
         (int)sizeof(int),
         (int)sizeof(long),
         (int)sizeof(void *),
         (int)sizeof(size_t));

  if ((data = (thread_data_t *)malloc(nb_threads * sizeof(thread_data_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  if ((threads = (pthread_t *)malloc(nb_threads * sizeof(pthread_t))) == NULL) {
    perror("malloc");
    exit(1);
  }

  stop = 0;

  /* Init STM */
  printf("Initializing STM\n");
  TM_INIT(nb_threads);

  /* Access set from all threads */
  barrier_init(&barrier, nb_threads + 1);
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  for (i = 0; i < nb_threads; i++) {
    printf("Creating thread %d\n", i);
		data[i].threadId = i;
    rand_init(data[i].seed);
    data[i].barrier = &barrier;
		data[i].nb_phases = nb_phases;
		data[i].phases = copy_phases(phases, nb_phases);
#ifdef THROUGHPUT_PROFILING
		int j;
		for (j = 0; j < nb_phases; j++) {
			data[i].phases[j].sampleCount = 0;
			data[i].phases[j].maxSamples = INIT_MAX_SAMPLES;
			int r = posix_memalign((void**)&(data[i].phases[j].throughputSamples), __CACHE_ALIGNMENT__, data[i].phases[j].maxSamples*sizeof(double));
			if ( r ) {
				perror("posix_memalign");
				fprintf(stderr, "error: failed to allocate samples array!\n"); \
				exit(EXIT_FAILURE);
			}
		}
#endif /* THROUGHPUT_PROFILING */
    if (pthread_create(&threads[i], &attr, test, (void *)(&data[i])) != 0) {
      fprintf(stderr, "Error creating thread\n");
      exit(1);
    }
  }
  pthread_attr_destroy(&attr);

  printf("STARTING...\n");
	for (i=0; i < nb_phases; i++){
  	gettimeofday(&start, NULL);
  	timeout.tv_sec = phases[i].duration / 1000;
  	timeout.tv_nsec = (phases[i].duration % 1000) * 1000000;
  	/* Start threads */
  	barrier_cross(&barrier);
  	if (phases[i].duration > 0) {
    	nanosleep(&timeout, NULL);
  	} else {
    	sigemptyset(&block_set);
    	sigsuspend(&block_set);
  	}
  	stop = 1;
  	gettimeofday(&end, NULL);
  	phases[i].duration = (end.tv_sec * 1000 + end.tv_usec / 1000) - (start.tv_sec * 1000 + start.tv_usec / 1000);
    printf("  #duration   : %d\n", phases[i].duration);
	}
  printf("STOPPING...\n");

  /* Wait for thread completion */
  for (i = 0; i < nb_threads; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      fprintf(stderr, "Error waiting for thread completion\n");
      exit(1);
    }
  }


  int duration = 0;
  unsigned long reads = 0;
  unsigned long updates = 0;
	printf("--------------------------------\n");
	for (j = 0; j < nb_phases; j++) {
		printf("Phase #%d\n", j);
  	for (i = 0; i < nb_threads; i++) {
			phases[j].nb_add      += data[i].phases[j].nb_add;
			phases[j].nb_remove   += data[i].phases[j].nb_remove;
			phases[j].nb_contains += data[i].phases[j].nb_contains;
			phases[j].nb_found    += data[i].phases[j].nb_found;
			phases[j].diff        += data[i].phases[j].diff;
   	}
		unsigned long phase_updates =  phases[j].nb_add + phases[j].nb_remove;
		unsigned long phase_reads   =  phases[j].nb_contains;
    printf("  #add        : %lu\n", phases[j].nb_add);
    printf("  #remove     : %lu\n", phases[j].nb_remove);
    printf("  #contains   : %lu\n", phases[j].nb_contains);
    printf("  #found      : %lu\n", phases[j].nb_found);
    printf("  #txs        : %lu (%f / s)\n", phase_reads + phase_updates,
		                                (phase_reads + phase_updates)*1000.0 / phases[j].duration);
    printf("  #read txs   : %lu (%f / s)\n", phase_reads,
		                                phase_reads * 1000.0 / phases[j].duration);
    printf("  #update txs : %lu (%f / s)\n", phase_updates,
		                                phase_updates * 1000.0 / phases[j].duration);
		unsigned long size = phases[j].initial + phases[j].diff;
    printf("  #duration   : %d\n", phases[j].duration);
  	printf("set size : %d (expected: %lu)\n", set_size(&(phases[j])), size);
		printf("--------------------------------\n");
		reads   += phase_reads;
   	updates += phase_updates;
		duration += phases[j].duration;
   	//size    += data[i].diff;
	}
	
  printf("Duration      : %d (ms)\n", duration);
  printf("#txs          : %lu (%f / s)\n", reads + updates, (reads + updates) * 1000.0 / duration );
  printf("#read txs     : %lu (%f / s)\n", reads, reads * 1000.0 / duration );
  printf("#update txs   : %lu (%f / s)\n", updates, updates * 1000.0 / duration );

  /* Cleanup STM */
  TM_EXIT(nb_threads);

#ifdef THROUGHPUT_PROFILING
	for (i=0; i < nb_phases; i++) {
		uint64_t maxSamples = 0;
		uint64_t j;
		for (j = 0; j < nb_threads; j++) {
			if (data[j].phases[i].maxSamples > maxSamples) {
				maxSamples = data[j].phases[i].maxSamples;
			}
		}
		phases[i].throughputSamples = (double*)calloc(sizeof(double), maxSamples);
		phases[i].sampleCount = UINT_MAX;
		for (j = 0; j < nb_threads; j++) {
			uint64_t sampleCount = data[j].phases[i].sampleCount;
			uint64_t k;
			for (k = 0; k < sampleCount; k++) {
				phases[i].throughputSamples[k] += data[j].phases[i].throughputSamples[k];
			}
			free(data[j].phases[i].throughputSamples);
			if ( data[j].phases[i].sampleCount < phases[i].sampleCount ) {
				phases[i].sampleCount = data[j].phases[i].sampleCount;
			}
		}
		uint64_t nSamples = phases[i].sampleCount;
		fprintf(outfile, "#%d %lu\n", i, nSamples);
		for (j = 0; j < nSamples; j++) {
			fprintf(outfile, "%0.3lf\n", phases[i].throughputSamples[j] );
		}
		free(phases[i].throughputSamples);
	}
	fclose(outfile);
#endif /* THROUGHPUT_PROFILING */

	// free thread-local phase data
  for (i = 0; i < nb_threads; i++) {
		free(data[i].phases);
	}
	for (j = 0; j < nb_phases; j++) {
  	/* Delete sets */
 	 	set_delete(&(phases[j]));
	}
	// free phase data
	free(phases);


  free(threads);
  free(data);

  return ret;
}
