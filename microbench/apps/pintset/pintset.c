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


#include <tm.h>

#ifdef DEBUG
# define IO_FLUSH                       fflush(NULL)
/* Note: stdio is thread-safe */
#endif

#include <common.h>
#include <linkedlist.h>
#include <hashset.h>

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
	static void SET_NAME##_test(thread_data_t *d, phase_data_t *p, SET_TYPE *set_ptr) \
	{ \
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
	  } \
	} \

STRESS_TEST(llistset, llistset_t)
STRESS_TEST(hashset, hashset_t)

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
				/*TODO*/
				fprintf(stderr,"error: red-black tree not implemented yet!\n");
				exit(-1);
				break;
		  case SL:
				/*TODO*/
				fprintf(stderr,"error: red-black tree not implemented yet!\n");
				exit(-1);
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
INIT_SET(hashset, hashset_t)

static void init_set(phase_data_t *p){
	switch(p->setImpl) {
		case LL:
			llistset_init(p, (llistset_t*)p->set_ptr);
			break;
		case HS:
			hashset_init(p, (hashset_t*)p->set_ptr);
			break;
		case RB:
			/*TODO*/
			fprintf(stderr,"error: red-black tree not implemented yet!\n");
			exit(-1);
			break;
	  case SL:
			/*TODO*/
			fprintf(stderr,"error: red-black tree not implemented yet!\n");
			exit(-1);
			break;
		default:
			fprintf(stderr,"error: invalid set type!\n");
			exit(-1);
			break;
	}
}

void* set_new(set_impl_t setImpl) {
	switch(setImpl) {
		case LL:
			return (void*)llistset_new();
			break;
		case HS:
			return (void*)hashset_new();
			break;
		case RB:
			/*TODO*/
			fprintf(stderr,"error: red-black tree not implemented yet!\n");
			exit(-1);
			break;
	  case SL:
			/*TODO*/
			fprintf(stderr,"error: red-black tree not implemented yet!\n");
			exit(-1);
			break;
		default:
			fprintf(stderr,"error: invalid set type!\n");
			exit(-1);
			break;
	}
}

void set_delete(phase_data_t *p) {
	switch(p->setImpl) {
		case LL:
			llistset_delete((llistset_t*)p->set_ptr);
			break;
		case HS:
			hashset_delete((hashset_t*)p->set_ptr);
			break;
		case RB:
			/*TODO*/
			fprintf(stderr,"error: red-black tree not implemented yet!\n");
			exit(-1);
			break;
	  case SL:
			/*TODO*/
			fprintf(stderr,"error: red-black tree not implemented yet!\n");
			exit(-1);
			break;
		default:
			fprintf(stderr,"error: invalid set type!\n");
			exit(-1);
			break;
	}
}

int set_size(phase_data_t* p) {
	switch(p->setImpl) {
		case LL:
			return llistset_size((llistset_t*)p->set_ptr);
			break;
		case HS:
			return hashset_size((hashset_t*)p->set_ptr);
			break;
		case RB:
			/*TODO*/
			fprintf(stderr,"error: red-black tree not implemented yet!\n");
			exit(-1);
			break;
	  case SL:
			/*TODO*/
			fprintf(stderr,"error: red-black tree not implemented yet!\n");
			exit(-1);
			break;
		default:
			fprintf(stderr,"error: invalid set type!\n");
			exit(-1);
			break;
	}
}

void init_phase_data(phase_data_t *p, int nb_phases){
	
	int i;
	for (i = 0; i < nb_phases; i++) {
		p[i].setImpl = LL;
		p[i].initial = DEFAULT_INITIAL;
		p[i].update = DEFAULT_UPDATE;
		p[i].range = 2*p[i].initial;
		p[i].alternate = DEFAULT_ALTERNATE;
	}
}

phase_data_t *copy_phases(phase_data_t* phases, int nb_phases){
	
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
    {"num-threads",               required_argument, NULL, 't'},
    {"number-of-phases",          required_argument, NULL, 'n'},
    {"phase-config",              required_argument, NULL, 'p'},
    {"duration",                  required_argument, NULL, 'd'},
    {"seed",                      required_argument, NULL, 's'},
    {NULL, 0, NULL, 0}
  };

	int size;
  int i, c, ret = 0;
  thread_data_t *data;
  pthread_t *threads;
  pthread_attr_t attr;
  barrier_t barrier;
  struct timeval start, end;
  struct timespec timeout;

  int nb_threads = DEFAULT_NB_THREADS;
  int duration = DEFAULT_DURATION;
  int seed = DEFAULT_SEED;
	
	int nb_phases  = DEFAULT_NB_PHASES;
	phase_data_t *phases = NULL;
  
	sigset_t block_set;

	int j = 0;
	char *buffer;
  while(1) {
  	i = 0;
    c = getopt_long(argc, argv, "h"
                    "t:n:p:d:s:"
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
              "        phase configuration string, ex: 'LL:4096:0' (default=NONE)\n"
        );
      	exit(0);
			case 't':
      	nb_threads = atoi(optarg);
      	break;
			case 'n':
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
			case 'd':
				duration = atoi(optarg);
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

  /* Thread-local seed for main thread */
  rand_init(main_seed);

  printf("Duration           : %d\n", duration);
	printf("Nb threads         : %d\n", nb_threads);
	printf("Seed               : %d\n", seed);
	printf("--------------------------------\n");
	for (i = 0; i < nb_phases; i++) {
		printf("Phase #%d\n", i);
  	printf("Set Implementation : %s\n", set_impl_map[phases[i].setImpl]);
 		printf("Value range        : %d\n", phases[i].range);
		printf("Update rate        : %d\n", phases[i].update);
		printf("Alternate          : %d\n", phases[i].alternate);
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
    if (pthread_create(&threads[i], &attr, test, (void *)(&data[i])) != 0) {
      fprintf(stderr, "Error creating thread\n");
      exit(1);
    }
  }
  pthread_attr_destroy(&attr);

  printf("STARTING...\n");
  gettimeofday(&start, NULL);
	int _i;
	for(_i=0; _i < nb_phases; _i++){
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
    printf("  #add        : %lu\n", phases[j].nb_add);
    printf("  #remove     : %lu\n", phases[j].nb_remove);
    printf("  #contains   : %lu\n", phases[j].nb_contains);
    printf("  #found      : %lu\n", phases[j].nb_found);
		unsigned long size = phases[j].initial + phases[j].diff;
  	printf("set size : %d (expected: %d)\n", set_size(&(phases[j])), size);
		printf("--------------------------------\n");
		reads   += phases[j].nb_contains;
   	updates += (phases[j].nb_add + phases[j].nb_remove);
   	//size    += data[i].diff;
	}
	
  printf("Duration      : %d (ms)\n", duration);
  printf("#txs          : %lu (%f / s)\n", reads + updates, (reads + updates) * 1000.0 / duration);
  printf("#read txs     : %lu (%f / s)\n", reads, reads * 1000.0 / duration);
  printf("#update txs   : %lu (%f / s)\n", updates, updates * 1000.0 / duration);

  /* Cleanup STM */
  TM_EXIT(nb_threads);

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
