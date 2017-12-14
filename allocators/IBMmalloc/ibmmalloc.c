/* =============================================================================
 *
 * memory.c
 * -- Very simple pseudo thread-local memory allocator
 *
 * Copyright (c) IBM Corp. 2014.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 * 
 *     * Neither the name of Stanford University nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY STANFORD UNIVERSITY ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL STANFORD UNIVERSITY BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * =============================================================================
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <string.h> // for memset
#include <stdbool.h>
#include <pthread.h>
#include <dlfcn.h> // for dlsym

#ifndef DEFAULT_INIT_BLOCK_CAPACITY
#define DEFAULT_INIT_BLOCK_CAPACITY (1 << 28) // 256Mb
#endif /*DEFAULT_INIT_BLOCK_CAPACITY */

#ifndef DEFAULT_BLOCK_GROWTH_FACTOR
#define DEFAULT_BLOCK_GROWTH_FACTOR 2
#endif /* DEFAULT_BLOCK_GROWTH_FACTOR */

#ifdef __370__
#define PADDING_SIZE 32
#elif defined(__bgq__)
#define PADDING_SIZE 16
#elif defined(__PPC__) || defined(_ARCH_PPC)
#define PADDING_SIZE 16
#elif defined(__x86_64__)
#define PADDING_SIZE 8
#else
#define PADDING_SIZE 8
#endif
typedef struct block {
    uint64_t padding1[PADDING_SIZE];
    size_t size;
    size_t capacity;
    char* contents;
    struct block* nextPtr;
    uint64_t padding2[PADDING_SIZE];
} block_t;

typedef struct pool {
    block_t* blocksPtr;
    size_t nextCapacity;
    size_t initBlockCapacity;
    long blockGrowthFactor;
} pool_t;

void init_lib(void);

/*
 * A list is used to store all the thread's memory pool pointers.
 * With this approach each thread can initialize its own memory
 * pool and it enables the main thread to free all memory pools
 * at the end of the application execution.
 *
 */
typedef struct _memoryPoolList {
	pool_t* pool;
	struct _memoryPoolList* next;
} memoryPoolList_t;

static void* (*real_malloc)(size_t size) = NULL;
static void  (*real_free)(void *ptr) = NULL;

static pthread_mutex_t globalPoolListLock = PTHREAD_MUTEX_INITIALIZER;
static memoryPoolList_t *memoryPoolListHead = NULL; // head of the list of all memory pools
static __thread pool_t *threadMemoryPool = NULL; // thread-local memory pool

static void
addMemoryPoolToPoolList(pool_t* pool){
	
	pthread_mutex_lock(&globalPoolListLock);

	if (memoryPoolListHead == NULL){
		memoryPoolListHead = (memoryPoolList_t*)real_malloc(sizeof(memoryPoolList_t));

		memoryPoolListHead->pool = pool;
		memoryPoolListHead->next = NULL;
	} else {

		memoryPoolList_t* p = memoryPoolListHead;

		while(p->next != NULL){
			p = p->next;
		}

		memoryPoolList_t* newNode = (memoryPoolList_t*)real_malloc(sizeof(memoryPoolList_t));

		newNode->pool = pool;
		newNode->next = NULL;

		p->next = newNode;
	}
	
	pthread_mutex_unlock(&globalPoolListLock);
}


/* =============================================================================
 * allocBlock
 * -- Returns NULL on failure
 * =============================================================================
 */
static block_t*
allocBlock (size_t capacity)
{
    block_t* blockPtr;

    assert(capacity > 0);

    blockPtr = (block_t*)real_malloc(sizeof(block_t));
    if (blockPtr == NULL) {
        return NULL;
    }

    blockPtr->size = 0;
    blockPtr->capacity = capacity;
    blockPtr->contents = (char*)real_malloc(capacity / sizeof(char) + 1);
    if (blockPtr->contents == NULL) {
        return NULL;
    }
    blockPtr->nextPtr = NULL;

    return blockPtr;
}


/* =============================================================================
 * freeBlock
 * =============================================================================
 */
static void
freeBlock (block_t* blockPtr)
{
    real_free(blockPtr->contents);
    real_free(blockPtr);
}


/* =============================================================================
 * allocPool
 * -- Returns NULL on failure
 * =============================================================================
 */
static pool_t*
allocPool (size_t initBlockCapacity, long blockGrowthFactor)
{
    pool_t* poolPtr;

    poolPtr = (pool_t*)real_malloc(sizeof(pool_t));
    if (poolPtr == NULL) {
        return NULL;
    }

    poolPtr->initBlockCapacity =
        (initBlockCapacity > 0) ? initBlockCapacity : DEFAULT_INIT_BLOCK_CAPACITY;
    poolPtr->blockGrowthFactor =
        (blockGrowthFactor > 0) ? blockGrowthFactor : DEFAULT_BLOCK_GROWTH_FACTOR;

    poolPtr->blocksPtr = allocBlock(poolPtr->initBlockCapacity);
    if (poolPtr->blocksPtr == NULL) {
        return NULL;
    }

    poolPtr->nextCapacity = poolPtr->initBlockCapacity *
                            poolPtr->blockGrowthFactor;

    return poolPtr;
}


/* =============================================================================
 * freeBlocks
 * =============================================================================
 */
static void
freeBlocks (block_t* blockPtr)
{
    if (blockPtr != NULL) {
        freeBlocks(blockPtr->nextPtr);
        freeBlock(blockPtr);
    }
}


/* =============================================================================
 * freePool
 * =============================================================================
 */
static void
freePool (pool_t* poolPtr)
{
    freeBlocks(poolPtr->blocksPtr);
    real_free(poolPtr);
}


/* =============================================================================
 * addBlockToPool
 * -- Returns NULL on failure, else pointer to new block
 * =============================================================================
 */
static block_t*
addBlockToPool (pool_t* poolPtr, long numByte)
{
    block_t* blockPtr;
    size_t capacity = poolPtr->nextCapacity;
    long blockGrowthFactor = poolPtr->blockGrowthFactor;

    if ((size_t)numByte > capacity) {
        capacity = numByte * blockGrowthFactor;
    }

    blockPtr = allocBlock(capacity);
    if (blockPtr == NULL) {
        return NULL;
    }

    blockPtr->nextPtr = poolPtr->blocksPtr;
    poolPtr->blocksPtr = blockPtr;
    poolPtr->nextCapacity = capacity * blockGrowthFactor;

    return blockPtr;
}


/* =============================================================================
 * getMemoryFromBlock
 * -- Reserves memory
 * =============================================================================
 */
static void*
getMemoryFromBlock (block_t* blockPtr, size_t numByte)
{
    size_t size = blockPtr->size;
    size_t capacity = blockPtr->capacity;

    assert((size + numByte) <= capacity);
    blockPtr->size += numByte;

    return (void*)&blockPtr->contents[size];
}


/* =============================================================================
 * getMemoryFromPool
 * -- Reserves memory
 * =============================================================================
 */
static void*
getMemoryFromPool (pool_t* poolPtr, size_t numByte)
{
    block_t* blockPtr = poolPtr->blocksPtr;

    if ((blockPtr->size + numByte) > blockPtr->capacity) {
#ifdef SIMULATOR
        assert(0);
#endif
        blockPtr = addBlockToPool(poolPtr, numByte);
        if (blockPtr == NULL) {
            return NULL;
        }
    }

    return getMemoryFromBlock(blockPtr, numByte);
}

/* =============================================================================
 * memory_init
 * -- Returns FALSE on failure
 * =============================================================================
 */
bool
memory_init (void) {
		
		threadMemoryPool = allocPool(0, 0); // initializes pool with default parameters
		if (threadMemoryPool == NULL) {
			return false;
		}

		addMemoryPoolToPoolList(threadMemoryPool);

    return true;
}


/* =============================================================================
 * memory_destroy
 * =============================================================================
 */
void
memory_destroy (void) {

		memoryPoolList_t* p = memoryPoolListHead;
		while(p != NULL){
			memoryPoolList_t* next = p->next;
			freePool(p->pool);
			real_free(p);
			p = next;
		}
}



/* =============================================================================
 * memory_get
 * -- Reserves memory
 * =============================================================================
 */
inline
static void*
memory_get (size_t numByte)
{
    pool_t* poolPtr;
    void* dataPtr;
    size_t addr;
    size_t misalignment;

#if DEBUG_IBM_MALLOC
		puts("called ibmmalloc()");
#endif /* DEBUG_IBM_MALLOC */

		assert(threadMemoryPool != NULL);
    poolPtr = threadMemoryPool;
    dataPtr = getMemoryFromPool(poolPtr, (numByte + 7)); /* +7 for alignment */

    /* Fix alignment for 64 bit */
    addr = (size_t)dataPtr;
    misalignment = addr % 8;
    if (misalignment) {
        addr += (8 - misalignment);
        dataPtr = (void*)addr;
    }

    return dataPtr;
}

/* =======================================================================
 *
 * Hacks and Tricks below.
 *
 * "Abandon hope all ye who enter here"
 *
 * =======================================================================
 */

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

void*
malloc (size_t size) {

	if ( unlikely( real_malloc == NULL ) ) {
		init_lib();
	}

	void* ret = memory_get(size);

	return ret;
}

void
free (void* addr) {

	// thread-local free is non-trivial, so do nothing.
	// memory will be freed after main function return
#if DEBUG_IBM_MALLOC
	puts("called ibmfree()");
#endif /* DEBUG_IBM_MALLOC */
}

typedef struct _pair {
	void* x;
	void* y;
} pair_t;

static void* start_routine_wrapper(void* a) {
	
	pair_t *p = (pair_t*)a;

	void* (*f)(void*) = p->x;
	void* arg  = p->y;
	
	bool status = memory_init();
	if (!status) {
		fprintf(stderr, "error: failed to initialize memory pool!\n");
		exit(EXIT_FAILURE);
	}

	// call the "real" start_routine of the thread
	return f(arg);
}


int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
									 void *(*start_routine) (void *), void *arg) {

	int (*real_pthread_create)(pthread_t *thread, const pthread_attr_t *attr,
									 void *(*start_routine) (void *), void *arg);
	real_pthread_create = dlsym(RTLD_NEXT, "pthread_create");
	if (real_pthread_create == NULL) {
		fprintf(stderr, "error: pthread_create function not loaded!\n");
		exit(EXIT_FAILURE);
	}

	pair_t *pair = (pair_t*)real_malloc(sizeof(pair_t));
	pair->x = start_routine;
	pair->y = arg;
	return real_pthread_create(thread,attr,start_routine_wrapper,(void*)pair);
}

__attribute__((constructor))
void
init_lib(void) {

	real_malloc = dlsym(RTLD_NEXT, "malloc");
	if (real_malloc == NULL) {
		fprintf(stderr, "error: malloc function not loaded!\n");
		exit(EXIT_FAILURE);
	}
	real_free = dlsym(RTLD_NEXT, "free");
	if (real_free == NULL) {
		fprintf(stderr, "error: free function not loaded!\n");
		exit(EXIT_FAILURE);
	}

	bool status = memory_init();
	if (!status) {
		fprintf(stderr, "error: failed to initialize memory pool!\n");
		exit(EXIT_FAILURE);
	}

#if DEBUG_IBM_MALLOC
	puts("init_lib exited!");
#endif /* DEBUG_IBM_MALLOC */
}

__attribute__((destructor))
void
term_lib(void) {
	
	memory_destroy();
#if DEBUG_IBM_MALLOC
	puts("term_lib exited!");
#endif /* DEBUG_IBM_MALLOC */
}

/* =============================================================================
 *
 * End of memory.c
 *
 * =============================================================================
 */
