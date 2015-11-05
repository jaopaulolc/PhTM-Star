
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <locks.h>
#include <htm.h>

long nthreads;
long num_iterations;
long x = 0;

static lock_t global_lock = 0;

void *foo(void *args){

	TX_INIT((long)args);
	long my_num_iterations = num_iterations/nthreads;

	long i;
	for(i = 0; i < my_num_iterations; i++){
		TX_START();
			x = x + 1;
		TX_END();
	}

	return NULL;
}

int main(int argc,char** argv){
	
	if(argc < 3){
		fprintf(stderr,"Usage: %s <nthreads> <niterations>\n",argv[0]);
		exit(EXIT_FAILURE);
	}

	nthreads = atol(argv[1]);
	num_iterations = atol(argv[2]);

	pthread_t *thread_handles = (pthread_t *)malloc(sizeof(pthread_t)*nthreads);

	HTM_STARTUP(nthreads);
	long i;
	for(i=0; i < nthreads; i++){
		if(pthread_create(&thread_handles[i],NULL,foo,(void*)i) != 0){
			perror("pthread_create");
		}
	}
	
	for(i=0; i < nthreads; i++){
		if(pthread_join(thread_handles[i],NULL) != 0){
			perror("pthread_join");
		}
	}
	free(thread_handles);
	
	HTM_SHUTDOWN();
	printf("\nx = %ld (verification = %s)\n",x,(x==num_iterations) ? "PASSED" : "FAILED");

	return 0;
}
