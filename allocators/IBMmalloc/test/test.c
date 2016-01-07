#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>

#define NUM_ALLOC (10)

__thread char* memArray[NUM_ALLOC];
long const nThreads = 4;

void*
work(void* arg) {

		long const tid = (long)arg;
    long i;
    long size;


    size = 1;
    for (i = 0; i < NUM_ALLOC; i++) {
        long j;
        printf("t%ld:Allocating %li bytes...\n", tid, size);
        memArray[i] = (char*)malloc(size);
        assert(memArray[i] != NULL);
        for (j = 0; j < size; j++) {
            memArray[i][j] = 'a' + (j % 26);
        }
        if (i % 2) {
            size = size * (i + 1);
        }
    }

    size = 1;
    for (i = 0; i < NUM_ALLOC; i++) {
        long j;
        printf("t%ld:Checking allocation of %li bytes...\n", tid, size);
        for (j = 0; j < size; j++) {
            assert(memArray[i][j] == 'a' + (j % 26));
        }
        if (i % 2) {
            size = size * (i + 1);
        }
				free(memArray[i]);
    }

		return NULL;
}

int
main (int argc, char** argv)
{
    long i;
    long size;

    puts("Starting tests...");

    size = 1;
    for (i = 0; i < NUM_ALLOC; i++) {
        long j;
        printf("main:Allocating %li bytes...\n", size);
        memArray[i] = (char*)malloc(size);
        assert(memArray[i] != NULL);
        for (j = 0; j < size; j++) {
            memArray[i][j] = 'a' + (j % 26);
        }
        if (i % 2) {
            size = size * (i + 1);
        }
    }

		pthread_t *threads = (pthread_t*)malloc(sizeof(pthread_t)*nThreads);
		for (i=0; i < nThreads; i++){
			int status = pthread_create(&threads[i], NULL, work, (void*)i);
			if(status){
				perror("pthread_create");
				exit(EXIT_FAILURE);
			}
		}
		
    size = 1;
    for (i = 0; i < NUM_ALLOC; i++) {
        long j;
        printf("main:Checking allocation of %li bytes...\n", size);
        for (j = 0; j < size; j++) {
            assert(memArray[i][j] == 'a' + (j % 26));
        }
        if (i % 2) {
            size = size * (i + 1);
        }
				free(memArray[i]);
    }
		
		for (i=0; i < nThreads; i++){
			int status = pthread_join(threads[i], NULL);
			if(status){
				perror("pthread_join");
				exit(EXIT_FAILURE);
			}
		}

		free(threads);

    puts("All tests passed.");

    return 0;
}
