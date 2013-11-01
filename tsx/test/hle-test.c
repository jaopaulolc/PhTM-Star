#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/* Requires TSX support in the CPU */
#include <hle.h>
hle_lock_t global_lock;

long nthreads;
long num_iterations;
long x = 0;

void *foo(void *args){
	
	long i;
	long my_num_iterations = num_iterations/nthreads;
	for(i=0; i < my_num_iterations; i++){
		hle_lock(&global_lock);
		x = x + 1;
		hle_unlock(&global_lock);
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
	
	printf("\nx = %ld (verification = %s)\n",x,(x==num_iterations) ? "PASSED" : "FAILED");

	return 0;
}
