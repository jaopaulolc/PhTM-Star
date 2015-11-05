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

#define L1_CACHE_SIZE   (32*1024)
#define L1_BLOCK_SIZE         64 // 64 bytes per block/line
#define L1_BLOCKS_PER_SET      8 // 8-way set associative
#define L1_NUM_SETS          (L1_CACHE_SIZE/(L1_BLOCK_SIZE*L1_BLOCKS_PER_SET))
#define L1_SET_BLOCK_OFFSET  ((L1_NUM_SETS*L1_BLOCK_SIZE)/sizeof(long))


#define PARAM_DEFAULT_NUMTHREADS (1L)
#define PARAM_DEFAULT_CONTENTION (100L)

#define __ALIGN__ __attribute__((aligned(0x1000)))

static pthread_t *threads __ALIGN__;
static long nThreads __ALIGN__ = PARAM_DEFAULT_NUMTHREADS;
static long pWrites  __ALIGN__ = PARAM_DEFAULT_CONTENTION;

static long volatile lastChoice;

static uint64_t *global_array __ALIGN__;
static int volatile stop __ALIGN__ = 0;
static pthread_barrier_t sync_barrier __ALIGN__ ;

void randomly_init_ushort_array(unsigned short *s, long n);
void parseArgs(int argc, char** argv);
void set_affinity(long id);
double getTimeSeconds();

void *readers_function(void *args){
	
	uint64_t const tid = (uint64_t)args;
	uint64_t const blocksPerThread = L1_BLOCKS_PER_SET/nThreads;
	unsigned short seed[3];
	randomly_init_ushort_array(seed, 3);

  TM_INIT_THREAD(tid);
	pthread_barrier_wait(&sync_barrier);
	
	while(!stop){
		
		uint64_t blockIndex __ALIGN__ = (nrand48(seed) % blocksPerThread) + tid*blocksPerThread;
		uint64_t j, i __ALIGN__ = blockIndex*L1_SET_BLOCK_OFFSET;
		__atomic_store_n(&lastChoice, i, __ATOMIC_RELEASE);
		TM_START(tid, RO);
			for(j=i; j < (i + L1_SET_BLOCK_OFFSET);j++){
				TM_LOAD(&global_array[j]);
			}
		TM_COMMIT;
	}

  TM_EXIT_THREAD(tid);
	return (void *)(tid);
}

void *writers_function(void *args){
	
	uint64_t const tid = (uint64_t)args;
	uint64_t const blocksPerThread = L1_BLOCKS_PER_SET/nThreads;
	unsigned short seed[3];
	randomly_init_ushort_array(seed,3);
	
  TM_INIT_THREAD(tid);
	pthread_barrier_wait(&sync_barrier);

	while(!stop){

		uint64_t volatile op __ALIGN__ = nrand48(seed) % 101;
		uint64_t blockIndex __ALIGN__ = (nrand48(seed) % blocksPerThread) + tid*blocksPerThread;
		uint64_t j, i __ALIGN__ = blockIndex*L1_SET_BLOCK_OFFSET;
		uint64_t const value = nrand48(seed) % (L1_CACHE_SIZE*L1_CACHE_SIZE);
		
		if(op < pWrites){
			i = __atomic_load_n(&lastChoice, __ATOMIC_ACQUIRE);
			TM_START(tid, RW);
				for(j=i; j < (i + L1_SET_BLOCK_OFFSET);j++){
					TM_STORE(&global_array[j], value);
				}
			TM_COMMIT;
		}
		else {
			__atomic_store_n(&lastChoice, i, __ATOMIC_RELEASE);
			TM_START(tid, RO);
				for(j=i; j < (i + L1_SET_BLOCK_OFFSET);j++){
					TM_LOAD(&global_array[j]);
				}
			TM_COMMIT;
		} 
	}

  TM_EXIT_THREAD(tid);
	return (void *)(tid);
}


int main(int argc, char** argv){

	parseArgs(argc, argv);
	
	struct timespec timeout = { .tv_sec = 1, .tv_nsec = 0 };
	pthread_barrier_init(&sync_barrier, NULL, nThreads+1);
	threads = (pthread_t*)malloc(sizeof(pthread_t)*nThreads);
	global_array = (uint64_t*)memalign(0x1000, L1_CACHE_SIZE);

	long nWriters = (nThreads  > 1) ? 2 : 1;
	TM_INIT(nThreads);
	
	long i;
	for(i=0; i < nThreads; i++){
		int status;
		if(nWriters){
			nWriters--;
			status = pthread_create(&threads[i],NULL,writers_function,(void*)i);
		} else {
			status = pthread_create(&threads[i],NULL,readers_function,(void*)i);
		}
		if(status != 0){
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
	printf("   u <LONG>   Contention level [0%%..100%%]     (%ld)\n", PARAM_DEFAULT_CONTENTION);

}

void parseArgs(int argc, char** argv){

	int opt;
	while( (opt = getopt(argc,argv,"n:u:")) != -1 ){
		switch(opt){
			case 'n':
				nThreads = strtol(optarg,NULL,10);
				break;
			case 'u':
				pWrites = strtol(optarg,NULL,10);
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
	CPU_SET(id, &cpuset);

	pthread_t current_thread = pthread_self();
	if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset)){
		perror("pthread_setaffinity_np");
		exit(EXIT_FAILURE);
	}
	
	while( id != sched_getcpu() );
}
