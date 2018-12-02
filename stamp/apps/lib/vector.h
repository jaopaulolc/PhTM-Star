/* =============================================================================
 *
 * vector.h
 *
 * =============================================================================
 *
 * Copyright (C) Stanford University, 2006.  All Rights Reserved.
 * Author: Chi Cao Minh
 *
 * =============================================================================
 *
 * For the license of bayes/sort.h and bayes/sort.c, please see the header
 * of the files.
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of kmeans, please see kmeans/LICENSE.kmeans
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of ssca2, please see ssca2/COPYRIGHT
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of lib/mt19937ar.c and lib/mt19937ar.h, please see the
 * header of the files.
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of lib/rbtree.h and lib/rbtree.c, please see
 * lib/LEGALNOTICE.rbtree and lib/LICENSE.rbtree
 * 
 * ------------------------------------------------------------------------
 * 
 * Unless otherwise noted, the following license applies to STAMP files:
 * 
 * Copyright (c) 2007, Stanford University
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


#ifndef VECTOR_H
#define VECTOR_H 1


#include "tm.h"
#include "types.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef struct vector {
    long size;
    long capacity;
    void** elements;
} vector_t;


/* =============================================================================
 * vector_alloc
 * -- Returns NULL if failed
 * =============================================================================
 */
vector_t*
vector_alloc (long initCapacity);


/* =============================================================================
 * Pvector_alloc
 * -- Returns NULL if failed
 * =============================================================================
 */
TM_SAFE
vector_t*
Pvector_alloc (long initCapacity);


/* =============================================================================
 * vector_free
 * =============================================================================
 */
void
vector_free (vector_t* vectorPtr);


/* =============================================================================
 * Pvector_free
 * =============================================================================
 */
TM_SAFE
void
Pvector_free (vector_t* vectorPtr);


/* =============================================================================
 * vector_at
 * -- Returns NULL if failed
 * =============================================================================
 */
TM_PURE
void*
vector_at (vector_t* vectorPtr, long i);

/* =============================================================================
 * Pvector_at
 * -- Returns NULL if failed
 * =============================================================================
 */
TM_SAFE
void*
Pvector_at (vector_t* vectorPtr, long i);

/* =============================================================================
 * vector_pushBack
 * -- Returns FALSE if fail, else TRUE
 * =============================================================================
 */
bool_t
vector_pushBack (vector_t* vectorPtr, void* dataPtr);


/* =============================================================================
 * Pvector_pushBack
 * -- Returns FALSE if fail, else TRUE
 * =============================================================================
 */
TM_SAFE
bool_t
Pvector_pushBack (vector_t* vectorPtr, void* dataPtr);


/* =============================================================================
 * vector_popBack
 * -- Returns NULL if fail, else returns last element
 * =============================================================================
 */
void*
vector_popBack (vector_t* vectorPtr);

/* =============================================================================
 * Pvector_popBack
 * -- Returns NULL if fail, else returns last element
 * =============================================================================
 */
TM_SAFE
void*
Pvector_popBack (vector_t* vectorPtr);

/* =============================================================================
 * vector_getSize
 * =============================================================================
 */
TM_PURE
long
vector_getSize (vector_t* vectorPtr);

/* =============================================================================
 * vector_getSize
 * =============================================================================
 */
TM_SAFE
long
Pvector_getSize (vector_t* vectorPtr);


/* =============================================================================
 * vector_clear
 * =============================================================================
 */
void
vector_clear (vector_t* vectorPtr);

/* =============================================================================
 * Pvector_clear
 * =============================================================================
 */
TM_SAFE
void
Pvector_clear (vector_t* vectorPtr);

/* =============================================================================
 * vector_sort
 * =============================================================================
 */
void
vector_sort (vector_t* vectorPtr, int (*compare) (const void*, const void*));

/* =============================================================================
 * Pvector_sort
 * =============================================================================
 */
TM_SAFE
void
Pvector_sort (vector_t* vectorPtr, int (*compare) (const void*, const void*));


/* =============================================================================
 * vector_copy
 * =============================================================================
 */
bool_t
vector_copy (vector_t* dstVectorPtr, vector_t* srcVectorPtr);


/* =============================================================================
 * Pvector_copy
 * =============================================================================
 */
TM_SAFE
bool_t
Pvector_copy (vector_t* dstVectorPtr, vector_t* srcVectorPtr);


#define PVECTOR_ALLOC(n)            Pvector_alloc(n)
#define PVECTOR_FREE(v)             Pvector_free(v)
#define PVECTOR_PUSHBACK(v, data)   Pvector_pushBack(v, data)
#define PVECTOR_POPBACK(v)          Pvector_popBack(v)
#define PVECTOR_AT(v, i)            Pvector_at(v, i)
#define PVECTOR_GETSIZE(v)          Pvector_getSize(v)
#define PVECTOR_CLEAR(v)            Pvector_clear(v)
#define PVECTOR_SORT(v, cmp)        Pvector_sort(v, cmp)
#define PVECTOR_COPY(dst, src)      Pvector_copy(dst, src)


#ifdef __cplusplus
}
#endif


#endif /* VECTOR_H */


/* =============================================================================
 *
 * End of vector.h
 *
 * =============================================================================
 */
