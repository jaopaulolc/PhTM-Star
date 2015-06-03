#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <libitm.h>
#include <stm.h>

#define NUM_THREADS 4

long sum = 0;

void* foo(void *ptr){

	_ITM_initializeThread();

	long x = (long)ptr;
	__transaction_atomic {
		sum += x;
	}
	
	_ITM_finalizeThread();

	return NULL;
}

int main(){
	_ITM_initializeProcess();

	pthread_t tid[NUM_THREADS];

	int i;
	for (i=0; i < NUM_THREADS; i++) {
		int ret = pthread_create(&tid[i], NULL, foo, (void*)1);
		if (ret != 0) {
			perror("pthread_create");
			exit(EXIT_FAILURE);
		}
	}

	for (i=0; i < NUM_THREADS; i++) {
		int ret = pthread_join(tid[i], NULL);
		if (ret != 0){
			perror("pthread_join");
			exit(EXIT_FAILURE);
		}
	}
	
	printf("sum = %ld\n",sum);

	_ITM_finalizeProcess();
	return 0;
}
