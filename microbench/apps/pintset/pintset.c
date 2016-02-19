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


#include <tm.h>

#ifdef DEBUG
# define IO_FLUSH                       fflush(NULL)
/* Note: stdio is thread-safe */
#endif

#include <common.h>
#include <linkedlist.h>
#include <hashset.h>

#define DEFAULT_DURATION                1000
#define DEFAULT_INITIAL                 256
#define DEFAULT_NB_THREADS              1
#define DEFAULT_RANGE                   (DEFAULT_INITIAL * 2)
#define DEFAULT_SEED                    0
#define DEFAULT_UPDATE                  20

#define XSTR(s)                         STR(s)
#define STR(s)                          #s


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

/* ################################################################### *
 * GLOBALS
 * ################################################################### */
static volatile int stop;
static unsigned short main_seed[3];

static inline void rand_init(unsigned short *seed)
{
  seed[0] = (unsigned short)rand();
  seed[1] = (unsigned short)rand();
  seed[2] = (unsigned short)rand();
}

static inline int rand_range(int n, unsigned short *seed)
{
  /* Return a random number in range [0;n) */
  int v = (int)(erand48(seed) * n);
  assert (v >= 0 && v < n);
  return v;
}

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

/* ################################################################### *
 * STRESS TEST
 * ################################################################### */

#define STRESS_TEST(SET_NAME, SET_TYPE) \
	static void SET_NAME##_test(thread_data_t *d, int range, SET_TYPE *set_ptr) \
	{ \
		int op, val, last = -1; \
	  while (stop == 0) { \
	    op = rand_range(100, d->seed); \
	    if (op < d->update) { \
	      if (d->alternate) { \
	        /* Alternate insertions and removals */ \
	        if (last < 0) { \
	          /* Add random value */ \
	          val = rand_range(range, d->seed) + 1; \
	          if (SET_NAME##_add(set_ptr, val, d)) { \
	            d->diff++; \
	            last = val; \
	          } \
	          d->nb_add++; \
	        } else { \
	          /* Remove last value */ \
	          if (SET_NAME##_remove(set_ptr, last, d)) \
	            d->diff--; \
	          d->nb_remove++; \
	          last = -1; \
	        } \
	      } else { \
	        /* Randomly perform insertions and removals */ \
	        val = rand_range(range, d->seed) + 1; \
	        if ((op & 0x01) == 0) { \
	          /* Add random value */ \
	          if (SET_NAME##_add(set_ptr, val, d)) \
	            d->diff++; \
	          d->nb_add++; \
	        } else { \
	          /* Remove random value */ \
	          if (SET_NAME##_remove(set_ptr, val, d)) \
	            d->diff--; \
	          d->nb_remove++; \
	        } \
	      } \
	    } else { \
	      /* Look for random value */ \
	      val = rand_range(range, d->seed) + 1; \
	      if (SET_NAME##_contains(set_ptr, val, d)) \
	        d->nb_found++; \
	      d->nb_contains++; \
	    } \
	  } \
	} \

STRESS_TEST(llistset, llistset_t)

static void *test(void *data)
{
  thread_data_t *d = (thread_data_t *)data;

  /* Create transaction */
  TM_INIT_THREAD(d->threadId);

  /* Wait on barrier */
  barrier_cross(d->barrier);
	llistset_test(d, d->large_range, d->large_llistset);
  /* Wait on barrier */
  barrier_cross(d->barrier);
	llistset_test(d, d->small_range, d->small_llistset);
  
	/* Free transaction */
  TM_EXIT_THREAD(d->threadId);

  return NULL;
}

int main(int argc, char **argv)
{
  struct option long_options[] = {
    // These options don't set a flag
    {"help",                      no_argument,       NULL, 'h'},
    {"do-not-alternate",          no_argument,       NULL, 'a'},
    {"duration",                  required_argument, NULL, 'd'},
    {"initial-size",              required_argument, NULL, 'i'},
    {"num-threads",               required_argument, NULL, 'n'},
    {"range",                     required_argument, NULL, 'r'},
    {"seed",                      required_argument, NULL, 's'},
    {"update-rate",               required_argument, NULL, 'u'},
    {NULL, 0, NULL, 0}
  };

  llistset_t *small_llistset;
  llistset_t *large_llistset;
	int small_size, large_size;
  int i, c, val, ret;
  unsigned long reads, updates;
  thread_data_t *data;
  pthread_t *threads;
  pthread_attr_t attr;
  barrier_t barrier;
  struct timeval start, end;
  struct timespec timeout;
  int duration = DEFAULT_DURATION;
  int small_initial = DEFAULT_INITIAL;
  int large_initial = DEFAULT_INITIAL;
  int nb_threads = DEFAULT_NB_THREADS;
  int small_range = DEFAULT_RANGE;
  int large_range = DEFAULT_RANGE;
  int seed = DEFAULT_SEED;
  int update = DEFAULT_UPDATE;
  int alternate = 1;
  sigset_t block_set;

	int noRange=1;
  while(1) {
    i = 0;
    c = getopt_long(argc, argv, "ha"
                    "d:i:n:r:s:u:"
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
              "  intset [options...]\n"
              "\n"
              "Options:\n"
              "  -h, --help\n"
              "        Print this message\n"
              "  -a, --do-not-alternate\n"
              "        Do not alternate insertions and removals\n"
	      "  -d, --duration <int>\n"
              "        Test duration in milliseconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
              "  -i, --initial-size <int>\n"
              "        Number of elements to insert before test (default=" XSTR(DEFAULT_INITIAL) ")\n"
              "  -n, --num-threads <int>\n"
              "        Number of threads (default=" XSTR(DEFAULT_NB_THREADS) ")\n"
              "  -r, --range <int>\n"
              "        Range of integer values inserted in set (default=" XSTR(DEFAULT_RANGE) ")\n"
              "  -s, --seed <int>\n"
              "        RNG seed (0=time-based, default=" XSTR(DEFAULT_SEED) ")\n"
              "  -u, --update-rate <int>\n"
              "        Percentage of update transactions (default=" XSTR(DEFAULT_UPDATE) ")\n"
         );
       exit(0);
     case 'a':
       alternate = 0;
       break;
     case 'd':
       duration = atoi(optarg);
       break;
     case 'i':
       large_initial = atoi(optarg);
       break;
     case 'n':
       nb_threads = atoi(optarg);
       break;
     case 'r':
       large_range = atoi(optarg);
			 noRange=0;
       break;
     case 's':
       seed = atoi(optarg);
       break;
     case 'u':
       update = atoi(optarg);
       break;
     case '?':
       printf("Use -h or --help for help\n");
       exit(0);
     default:
       exit(1);
    }
  }

	small_initial = large_initial/16;
	if(noRange){
		small_range=2*small_initial;
		large_range=2*large_initial;
	}

  assert(duration >= 0);
  assert(large_initial >= 0);
  assert(nb_threads > 0);
  assert(large_range > 0 && large_range >= large_initial);
  assert(update >= 0 && update <= 100);

  printf("Duration           : %d\n", duration);
  printf("Initial small_size : %d\n", small_initial);
  printf("Initial large_size : %d\n", large_initial);
  printf("Nb threads         : %d\n", nb_threads);
  printf("Value small_range  : %d\n", small_range);
  printf("Value large_range  : %d\n", large_range);
  printf("Seed               : %d\n", seed);
  printf("Update rate        : %d\n", update);
  printf("Alternate          : %d\n", alternate);
  printf("Type sizes         : int=%d/long=%d/ptr=%d/word=%d\n",
         (int)sizeof(int),
         (int)sizeof(long),
         (int)sizeof(void *),
         (int)sizeof(size_t));

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

  if (seed == 0)
    srand((int)time(NULL));
  else
    srand(seed);

  small_llistset = llistset_new();
  large_llistset = llistset_new();

  stop = 0;

  /* Thread-local seed for main thread */
  rand_init(main_seed);

  /* Init STM */
  printf("Initializing STM\n");
  TM_INIT(nb_threads);
  if (alternate == 0 && large_range != large_initial * 2)
    printf("WARNING: range is not twice the initial set size\n");

  /* Populate set */
  printf("Adding %d entries to small_llistset\n", small_initial);
  i = 0;
  while (i < small_initial) {
    val = rand_range(small_range, main_seed) + 1;
    if (llistset_add(small_llistset, val, 0))
      i++;
  }
  printf("Adding %d entries to large_llistset\n", large_initial);
  i = 0;
  while (i < large_initial) {
    val = rand_range(large_range, main_seed) + 1;
    if (llistset_add(large_llistset, val, 0))
      i++;
  }
  
	small_size = llistset_size(small_llistset);
	large_size = llistset_size(large_llistset);
  printf("small_llistset size : %d\n", small_size);
  printf("large_llistset size : %d\n", large_size);

  /* Access set from all threads */
  barrier_init(&barrier, nb_threads + 1);
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  for (i = 0; i < nb_threads; i++) {
    printf("Creating thread %d\n", i);
		data[i].threadId = i;
    data[i].update = update;
    data[i].alternate = alternate;
    data[i].nb_add = 0;
    data[i].nb_remove = 0;
    data[i].nb_contains = 0;
    data[i].nb_found = 0;
    data[i].diff = 0;
    rand_init(data[i].seed);
    data[i].small_range = small_range;
    data[i].large_range = large_range;
    data[i].small_llistset = small_llistset;
    data[i].large_llistset = large_llistset;
    data[i].barrier = &barrier;
    if (pthread_create(&threads[i], &attr, test, (void *)(&data[i])) != 0) {
      fprintf(stderr, "Error creating thread\n");
      exit(1);
    }
  }
  pthread_attr_destroy(&attr);

  printf("STARTING...\n");
  gettimeofday(&start, NULL);
	int _i;
	for(_i=0; _i < 2; _i++){
  	/* Start threads */
  	barrier_cross(&barrier);
  	if (duration > 0) {
    	nanosleep(&timeout, NULL);
  	} else {
    	sigemptyset(&block_set);
    	sigsuspend(&block_set);
  	}
  	stop = 1;
  timeout.tv_sec = duration / 1000;
  timeout.tv_nsec = (duration % 1000) * 1000000;
	}
  gettimeofday(&end, NULL);
  printf("STOPPING...\n");

  /* Wait for thread completion */
  for (i = 0; i < nb_threads; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      fprintf(stderr, "Error waiting for thread completion\n");
      exit(1);
    }
  }

  duration = (end.tv_sec * 1000 + end.tv_usec / 1000) - (start.tv_sec * 1000 + start.tv_usec / 1000);
  reads = 0;
  updates = 0;
  for (i = 0; i < nb_threads; i++) {
    printf("Thread %d\n", i);
    printf("  #add        : %lu\n", data[i].nb_add);
    printf("  #remove     : %lu\n", data[i].nb_remove);
    printf("  #contains   : %lu\n", data[i].nb_contains);
    printf("  #found      : %lu\n", data[i].nb_found);
    reads += data[i].nb_contains;
    updates += (data[i].nb_add + data[i].nb_remove);
    small_size += data[i].diff;
    large_size += data[i].diff;
  }
  printf("small_llistset size : %d (expected: %d)\n", llistset_size(small_llistset), small_size);
  printf("large_llistset size : %d (expected: %d)\n", llistset_size(large_llistset), large_size);
  ret = (llistset_size(small_llistset) != small_size || llistset_size(large_llistset) != large_size);
  printf("Duration      : %d (ms)\n", duration);
  printf("#txs          : %lu (%f / s)\n", reads + updates, (reads + updates) * 1000.0 / duration);
  printf("#read txs     : %lu (%f / s)\n", reads, reads * 1000.0 / duration);
  printf("#update txs   : %lu (%f / s)\n", updates, updates * 1000.0 / duration);

  /* Delete sets */
  llistset_delete(small_llistset);
  llistset_delete(large_llistset);

  /* Cleanup STM */
  TM_EXIT(nb_threads);

  free(threads);
  free(data);

  return ret;
}
