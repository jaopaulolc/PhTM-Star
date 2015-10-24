#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>

#include <tm.h>

#define RO	1
#define RW	0

#define L1_CACHE_SIZE    32*1024 
#define L1_BLOCK_SIZE         64 // 64 bytes per block/line
#define L1_BLOCKS_PER_SET      8 // 8-way set associative
#define L1_NUM_SETS      (L1_CACHE_SIZE/(L1_BLOCK_SIZE*L1_BLOCKS_PER_SET))


#define PARAM_DEFAULT_NUMTHREADS   (1L)
#define PARAM_DEFAULT_OVERFLOWPROB (10L)

static pthread_t *threads;
static long nThreads   = PARAM_DEFAULT_NUMTHREADS;
static long pOverflow  = PARAM_DEFAULT_OVERFLOWPROB;

static long *global_array;
static int volatile stop = 0;
static pthread_barrier_t sync_barrier;

void randomly_init_ushort_array(unsigned short *s, long n);
void parseArgs(int argc, char** argv);
void set_affinity(long id);

void *writers_function(void *args){
	
	long tid = (long)args;
	long const step = (L1_BLOCK_SIZE/sizeof(long))*L1_NUM_SETS;
	long const setStart = tid*(L1_NUM_SETS/nThreads);
	
	unsigned short seed[3];
	randomly_init_ushort_array(seed,3);
	
  TM_INIT_THREAD(tid);
	pthread_barrier_wait(&sync_barrier);

	while(!stop){

		long op = nrand48(seed) % 101;
		long set = (nrand48(seed) % L1_NUM_SETS)/nThreads + setStart;
		
		if(pOverflow < op){
			TM_START(tid, RW);
				long i;
				for (i=0; i < L1_BLOCKS_PER_SET - 4; i++){
					long j;
					for(j=0; j < L1_BLOCK_SIZE/sizeof(long); j++){
						TM_STORE(&global_array[i*step + j + set*8], 0);
					}
				}
			TM_COMMIT;
		} else if(pOverflow > op) {
			TM_START(tid, RW);
				long i;
				for (i=0; i < L1_BLOCKS_PER_SET + 1; i++){
					long j;
					for(j=0; j < L1_BLOCK_SIZE/sizeof(long); j++){
						TM_STORE(&global_array[i*step + j + set*8], 0);
					}
				}
			TM_COMMIT;
		}
	}

  TM_EXIT_THREAD(tid);
	return (void *)(tid);
}


int main(int argc, char** argv){

	parseArgs(argc, argv);
	
	threads = (pthread_t*)malloc(sizeof(pthread_t)*nThreads);

	global_array = (long*)memalign(0x1000, (L1_CACHE_SIZE + 512) * sizeof(long));
	
	memset(global_array, 0, (L1_CACHE_SIZE + 512) * sizeof(long));

	pthread_barrier_init(&sync_barrier, NULL, nThreads+1);

	struct timespec timeout = { 
		.tv_sec  = 1 ,
		.tv_nsec = 0
	};

	TM_INIT(nThreads);
	
	long i;
	for(i=0; i < nThreads; i++){
		if(pthread_create(&threads[i],NULL,writers_function,(void*)i)){
			perror("pthread_create");
			exit(EXIT_FAILURE);
		}
	}

	pthread_barrier_wait(&sync_barrier);
	printf("STARTING...\n");

	nanosleep(&timeout,NULL);
	stop = 1;

	printf("STOPING...\n");

	void **ret_thread = (void**)malloc(sizeof(void*)*nThreads);

	for(i=0; i < nThreads; i++){
		if(pthread_join(threads[i],&ret_thread[i])){
			perror("pthread_join");
			exit(EXIT_FAILURE);
		}
	}
	free(ret_thread);
	
	TM_EXIT(nThreads);

	free(threads);
	free(global_array);
	
	return 0;
}

void randomly_init_ushort_array(unsigned short *s, long n){
	
	srand(time(NULL));
	long i;
	for(i=0; i < n; i++){
		s[i] = (unsigned short)rand();
	}
}

void showUsage(const char* argv0){
	
	printf("\nUsage %s [options]\n",argv0);
	printf("Options:                                      (defaults)\n");
	printf("   n <LONG>   Number of threads                 (%ld)\n", PARAM_DEFAULT_NUMTHREADS);
	printf("   u <LONG>   Overflow probability [0%%..100%%] (%ld)\n", PARAM_DEFAULT_OVERFLOWPROB);

}


void parseArgs(int argc, char** argv){

	int opt;
	while( (opt = getopt(argc,argv,"n:u:")) != -1 ){
		switch(opt){
			case 'n':
				nThreads = strtol(optarg,NULL,10);
				break;
			case 'u':
				pOverflow = strtol(optarg,NULL,10);
				break;
			default:
				showUsage(argv[0]);
				exit(EXIT_FAILURE);
		}	
	}
}

void set_affinity(long id){
	int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
	if (id < 0 || id >= num_cores){
		fprintf(stderr,"error: invalid number of threads (nthreads > ncores)!\n");
		exit(EXIT_FAILURE);
	}
	
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(id%num_cores, &cpuset);

	pthread_t current_thread = pthread_self();
	if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset)){
		perror("pthread_setaffinity_np");
		exit(EXIT_FAILURE);
	}
}
