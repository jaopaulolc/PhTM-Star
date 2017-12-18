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

#include <common.h>
#include <rbtree.h>
#include <rbforest.h>

#define DEFAULT_NB_TREES              350
#define DEFAULT_INITIAL               4096
#define DEFAULT_DURATION              1000
#define DEFAULT_NB_THREADS            1
#define DEFAULT_RANGE                 (DEFAULT_INITIAL * 2)
#define DEFAULT_SEED                  0
#define DEFAULT_UPDATE                20

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

#define TARGET_NUM_SAMPLES 1000
#define INIT_MAX_SAMPLES (2*TARGET_NUM_SAMPLES)

#include <unistd.h>
#include <sched.h>
void set_affinity(long id){
	int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
	if (id < 0 || id >= num_cores) {
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

#define THROUGHPUT_PROFILING_VARS \
		uint64_t stepCount; \
		stepCount = 0; \
		uint64_t sample_step; \
		sample_step = TARGET_NUM_SAMPLES; \
		uint64_t before; \
		before = getTime(); \
		double *throughputSamples = d->throughputSamples; 

#define THROUGHPUT_PROFILING_CODE \
			if (stepCount == sample_step) { \
				uint64_t now = getTime(); \
				double t = now - before; \
				double th = (sample_step*1.0e9)/t; \
				throughputSamples[d->sampleCount] = th; \
				sample_step = ((uint64_t)th)/TARGET_NUM_SAMPLES; \
				d->sampleCount++; \
				if ( d->sampleCount == d->maxSamples ) { \
					increaseThroughputSamplesSize(&(d->throughputSamples), &(d->maxSamples), 2*d->maxSamples); \
					throughputSamples = d->throughputSamples; \
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

static void init_forest(forest_t* forest, int initial, int range) {
  int i = 0;
	size_t nb_trees = forest_nb_trees(forest);
  while (i < initial) {
    int val = rand_range(range, main_seed) + 1;
    if (forest_add(forest, val, nb_trees, NULL))
      i++;
  }
}

/* ################################################################### *
 * STRESS TEST
 * ################################################################### */

static void *test(void *data)
{
  int op, val, last = -1, k = 1;
  thread_data_t *d = (thread_data_t *)data;

  /* Create transaction */
  TM_INIT_THREAD(d->threadId);
  /* Wait on barrier */
  barrier_cross(d->barrier);

	size_t nb_trees = forest_nb_trees(d->forest);

	THROUGHPUT_PROFILING_VARS

  while (stop == 0) {
    op = rand_range(100, d->seed);
    if (op < d->update) {
    	/* Alternate insertions and removals */
      if (last < 0) {
        /* Add random value to first k trees */
        val = rand_range(d->range, d->seed) + 1;
				k = rand_range_non_zero(nb_trees, d->seed);
        if (forest_add(d->forest, val, k, d)) {
          d->diff++;
          last = val;
        }
        d->nb_add++;
      } else {
        /* Remove last value */
        if (forest_remove(d->forest, last, k, d))
          d->diff--;
        d->nb_remove++;
        last = -1;
      }
    } else {
			size_t k;
      /* Look for random value */
      val = rand_range(d->range, d->seed) + 1;
			k = rand_range_non_zero(nb_trees, d->seed);
      if (forest_contains(d->forest, val, k, d))
        d->nb_found++;
      d->nb_contains++;
    }
		THROUGHPUT_PROFILING_CODE
  }

  /* Free transaction */
  TM_EXIT_THREAD(d->threadId);

  return NULL;
}

int main(int argc, char **argv)
{
  struct option long_options[] = {
    // These options don't set a flag
    {"help",                      no_argument,       NULL, 'h'},
    {"num-threads",               required_argument, NULL, 'n'},
    {"number-of-trees",           required_argument, NULL, 't'},
    {"duration",                  required_argument, NULL, 'd'},
    {"initial-size",              required_argument, NULL, 'i'},
    {"range",                     required_argument, NULL, 'r'},
    {"update-rate",               required_argument, NULL, 'u'},
    {"seed",                      required_argument, NULL, 's'},
    {NULL, 0, NULL, 0}
  };

  int i, c;
  thread_data_t *data;
  pthread_t *threads;
  pthread_attr_t attr;
  barrier_t barrier;
  struct timeval start, end;
  struct timespec timeout;

  int nb_threads = DEFAULT_NB_THREADS;
	int nb_trees  = DEFAULT_NB_TREES;
	int duration = DEFAULT_DURATION;
	int initial = DEFAULT_INITIAL;
  int range = DEFAULT_RANGE;
  int update = DEFAULT_UPDATE;
  int seed = DEFAULT_SEED;

	forest_t* forest;
  
	sigset_t block_set;

#ifdef THROUGHPUT_PROFILING
	FILE *outfile = fopen("transactions.throughput", "w");
	if ( outfile == NULL ) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}
#endif /* THROUGHPUT_PROFILING */

  while(1) {
  	i = 0;
    c = getopt_long(argc, argv, "h"
				"n:t:d:i:r:u:s:", long_options, &i);

    if(c == -1)
      break;

    if(c == 0 && long_options[i].flag == 0)
      c = long_options[i].val;

    switch(c) {
    	case 0:
      	/* Flag is automatically set */
      	break;
    	case 'h':
      	printf("forest -- STM stress test "
              "\n"
              "Usage:\n"
              "  forest [options...]\n"
              "\n"
              "Options:\n"
              "  -h, --help\n"
              "        Print this message\n"
              "  -n, --num-threads <int>\n"
              "        Number of threads (default=" XSTR(DEFAULT_NB_THREADS) ")\n"
              "  -t, --number-of-sets <int>\n"
              "        Number of tree in the forest (default=" XSTR(DEFAULT_NB_TREES) ")\n"
	      			"  -d, --duration <int>\n"
              "        Test duration in milliseconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
              "  -i, --initial-size <int>\n"
              "        Number of elements to insert before test (default=" XSTR(DEFAULT_INITIAL) ")\n"
              "  -r, --range <int>\n"
              "        Range of integer values inserted in set (default=" XSTR(DEFAULT_RANGE) ")\n"
              "  -u, --update-rate <int>\n"
              "        Percentage of update transactions (default=" XSTR(DEFAULT_UPDATE) ")\n"
              "  -s, --seed <int>\n"
              "        RNG seed (0=time-based, default=" XSTR(DEFAULT_SEED) ")\n"
        );
      	exit(0);
			case 'n':
      	nb_threads = atoi(optarg);
      	break;
			case 't':
				nb_trees = atoi(optarg);
				break;
			case 'd':
				duration = atoi(optarg);
				break;
			case 'i':
				initial = atoi(optarg);
				break;
			case 'r':
				range = atoi(optarg);
				break;
			case 'u':
				update = atoi(optarg);
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

  if (seed == 0)
		srand((int)time(NULL));
  else
		srand(seed);

  /* Thread-local seed for main thread */
  rand_init(main_seed);

	printf("Nb threads         : %d\n", nb_threads);
	printf("Seed               : %d\n", seed);
	printf("Nb trees           : %d\n", nb_trees);
	printf("--------------------------------\n");
 	printf("Value range        : %d\n", range);
	printf("Update rate        : %d\n", update);
	//printf("Alternate          : %d\n", sets[i].alternate);
 	printf("Duration           : %d\n", duration);
 	/* Populate set */
 	printf("Initial size       : %d * %d = %d\n", initial, nb_trees, initial*nb_trees);
 	if ( range != initial * 2 )
  	printf("WARNING: range is not twice the initial set size\n");
	printf("--------------------------------\n");

  printf("Type sizes         : int=%d/long=%d/ptr=%d/word=%d\n",
         (int)sizeof(int),
         (int)sizeof(long),
         (int)sizeof(void *),
         (int)sizeof(size_t));
	
	forest = forest_new(nb_trees);
	init_forest(forest, initial, range);

  
	timeout.tv_sec = duration / 1000;
  timeout.tv_nsec = (duration % 1000) * 1000000;

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
  barrier_init(&barrier, nb_threads);
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  for (i = 0; i < nb_threads; i++) {
    printf("Creating thread %d\n", i);
		data[i].threadId = i;
    data[i].range = range;
    data[i].update = update;
    data[i].nb_add = 0;
    data[i].nb_remove = 0;
    data[i].nb_contains = 0;
    data[i].nb_found = 0;
    data[i].diff = 0;
    rand_init(data[i].seed);
    data[i].barrier = &barrier;
		data[i].forest = forest;
#ifdef THROUGHPUT_PROFILING
		data[i].sampleCount = 0;
		data[i].maxSamples = INIT_MAX_SAMPLES;
		int r = posix_memalign((void**)&(data[i].throughputSamples), __CACHE_ALIGNMENT__, INIT_MAX_SAMPLES*sizeof(double));
		if ( r ) {
			perror("posix_memalign");
			fprintf(stderr, "error: failed to allocate samples array!\n"); \
			exit(EXIT_FAILURE);
		}
#endif /* THROUGHPUT_PROFILING */
    if (pthread_create(&threads[i], &attr, test, (void *)(&data[i])) != 0) {
      fprintf(stderr, "Error creating thread\n");
      exit(1);
    }
  }
  pthread_attr_destroy(&attr);
  
	printf("STARTING...\n");
  gettimeofday(&start, NULL);
  if (duration > 0) {
    nanosleep(&timeout, NULL);
  } else {
    sigemptyset(&block_set);
    sigsuspend(&block_set);
  }
  stop = 1;
  gettimeofday(&end, NULL);
  printf("STOPPING...\n");
  
	duration = (end.tv_sec * 1000 + end.tv_usec / 1000) - (start.tv_sec * 1000 + start.tv_usec / 1000);

  /* Wait for thread completion */
  for (i = 0; i < nb_threads; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      fprintf(stderr, "Error waiting for thread completion\n");
      exit(1);
    }
  }

	unsigned long diff = 0;
  unsigned long reads = 0;
  unsigned long updates = 0;
	printf("--------------------------------\n");
  for (i = 0; i < nb_threads; i++) {
    printf("Thread %d\n", i);
    printf("  #add        : %lu\n", data[i].nb_add);
    printf("  #remove     : %lu\n", data[i].nb_remove);
    printf("  #contains   : %lu\n", data[i].nb_contains);
    printf("  #found      : %lu\n", data[i].nb_found);
    reads += data[i].nb_contains;
    updates += (data[i].nb_add + data[i].nb_remove);
    diff += data[i].diff;
  }
  printf("Duration      : %d (ms)\n", duration);
  printf("#txs          : %lu (%f / s)\n", reads + updates, (reads + updates) * 1000.0 / duration);
  printf("#read txs     : %lu (%f / s)\n", reads, reads * 1000.0 / duration);
  printf("#update txs   : %lu (%f / s)\n", updates, updates * 1000.0 / duration);
	printf("Set size      : %d (expected: %lu)\n", forest_size(forest), initial*nb_trees + diff);

  /* Cleanup STM */
  TM_EXIT(nb_threads);

#ifdef THROUGHPUT_PROFILING
	uint64_t maxSamples = 0;
	uint64_t sampleCount = ULONG_MAX;
	uint64_t j;
	for (j = 0; j < nb_threads; j++) {
		if (data[j].maxSamples < maxSamples) {
			maxSamples = data[j].maxSamples;
		}
	}
	double* throughputSamples = (double*)calloc(sizeof(double), maxSamples);
	for (j = 0; j < nb_threads; j++) {
		uint64_t k;
		for (k = 0; k < data[j].sampleCount; k++) {
			throughputSamples[k] += data[j].throughputSamples[k];
		}
		free(data[j].throughputSamples);
		if ( data[j].sampleCount < sampleCount ) {
			sampleCount = data[j].sampleCount;
		}
	}
	uint64_t nSamples = sampleCount;
	fprintf(outfile, "#%d %lu\n", 1, nSamples);
	for (j = 0; j < nSamples; j++) {
		fprintf(outfile, "%0.3lf\n", throughputSamples[j]);
	}
	free(throughputSamples);
	fclose(outfile);
#endif /* THROUGHPUT_PROFILING */

	forest_delete(forest);
  free(threads);
  free(data);

  return 0;
}
