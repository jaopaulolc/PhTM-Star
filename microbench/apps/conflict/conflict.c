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

static pthread_t *threads;
static long nThreads     = PARAM_DEFAULT_NUMTHREADS;
static uint64_t pWrites  = PARAM_DEFAULT_CONTENTION;

static long volatile lastChoice;

static uint64_t *global_array;
static int volatile stop = 0;
static pthread_barrier_t sync_barrier;

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
	
	uint64_t sum = 0;
	while(!stop){
		
		uint64_t setIndex = (nrand48(seed) % blocksPerThread) + tid*blocksPerThread;
		//uint64_t setIndex = nrand48(seed) % 8;
		uint64_t j, i = setIndex*L1_SET_BLOCK_OFFSET;
		lastChoice = i;
		TM_START(tid, RO);
			for(j=i; j < (i + L1_SET_BLOCK_OFFSET);j++){
				TM_LOAD(&global_array[j]);
			}
		TM_COMMIT;
	}

  TM_EXIT_THREAD(tid);
	return (void *)(sum*tid);
}

void *writer_function(void *args){
	
	uint64_t const tid = (uint64_t)args;
	uint64_t const blocksPerThread = L1_BLOCKS_PER_SET/nThreads;
	unsigned short seed[3];
	randomly_init_ushort_array(seed,3);
	
  TM_INIT_THREAD(tid);
	pthread_barrier_wait(&sync_barrier);

	uint64_t sum = 0;
	while(!stop){

		uint64_t volatile op = nrand48(seed) % 101;
		uint64_t setIndex = (nrand48(seed) % blocksPerThread) + tid*blocksPerThread;
		//uint64_t setIndex = nrand48(seed) % 8;
		uint64_t j, i = setIndex*L1_SET_BLOCK_OFFSET;
		uint64_t const value = nrand48(seed) % (L1_CACHE_SIZE*L1_CACHE_SIZE);
		
		if(op < pWrites){
			i = lastChoice;
			TM_START(tid, RW);
				for(j=i; j < (i + L1_SET_BLOCK_OFFSET);j++){
					TM_STORE(&global_array[j], value);
				}
			TM_COMMIT;
		}
		else {
			TM_START(tid, RO);
				for(j=i; j < (i + L1_SET_BLOCK_OFFSET);j++){
					TM_LOAD(&global_array[j]);
				}
			TM_COMMIT;
		} 
	}

  TM_EXIT_THREAD(tid);
	return (void *)(sum*tid);
}


int main(int argc, char** argv){

	parseArgs(argc, argv);
	
	struct timespec timeout = { .tv_sec = 1, .tv_nsec = 0 };
	pthread_barrier_init(&sync_barrier, NULL, nThreads+1);
	threads = (pthread_t*)malloc(sizeof(pthread_t)*nThreads);
	global_array = (uint64_t*)memalign(0x1000, L1_CACHE_SIZE);

	uint64_t set;
	for (set=0; set < L1_NUM_SETS; set++){
		uint64_t word;
		for (word=0; word < L1_BLOCK_SIZE/sizeof(long); word++){
			uint64_t block;
			for (block=0; block < L1_BLOCKS_PER_SET; block++){
				uint64_t addr = (uint64_t)&global_array[block*512 + word + set*8];
				uint64_t set_n = (addr >> 6) & 0x3F;
				if(set_n != set) {
					fprintf(stderr,"error: set != set_n!\n");
					exit(EXIT_FAILURE);
				}
				global_array[block*512 + word + set*8] = rand() % L1_CACHE_SIZE;
			}
		}
	}

	TM_INIT(nThreads);
	
	long i;
	for(i=1; i < nThreads; i++){
		if(pthread_create(&threads[i],NULL,readers_function,(void*)i)){
			perror("pthread_create");
			exit(EXIT_FAILURE);
		}
	}

	if(pthread_create(&threads[0],NULL,writer_function,(void*)0)){
		perror("pthread_create");
		exit(EXIT_FAILURE);
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
}
