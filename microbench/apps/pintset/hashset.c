/* ################################################################### *
 * HASHSET
 * ################################################################### */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hashset.h>

# define INIT_SET_PARAMETERS            /* Nothing */
# define NB_BUCKETS                     (1UL << 17)
# define HASH(a)                        (hash((uint32_t)a) & (NB_BUCKETS - 1))

typedef struct bucket {
  val_t val;
  struct bucket *next;
} bucket_t;

struct hashset {
  bucket_t **buckets;
};


TM_PURE
static uint32_t hash(uint32_t a)
{
  /* Knuth's multiplicative hash function */
  a *= 2654435761UL;
  return a;
}

TM_SAFE
static bucket_t *new_entry(val_t val, bucket_t *next, int transactional)
{
  bucket_t *b;

  if (!transactional) {
    b = (bucket_t *)malloc(sizeof(bucket_t));
  } else {
    b = (bucket_t *)TM_MALLOC(sizeof(bucket_t));
  }
  if (b == NULL) {
    perror("malloc");
    exit(1);
  }

  b->val = val;
  b->next = next;

  return b;
}

hashset_t *hashset_new()
{
  hashset_t *set;

  if ((set = (hashset_t *)malloc(sizeof(hashset_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  if ((set->buckets = (bucket_t **)calloc(NB_BUCKETS, sizeof(bucket_t *))) == NULL) {
    perror("calloc");
    exit(1);
  }

  return set;
}

void hashset_delete(hashset_t *set)
{
  unsigned int i;
  bucket_t *b, *next;

  for (i = 0; i < NB_BUCKETS; i++) {
    b = set->buckets[i];
    while (b != NULL) {
      next = b->next;
      free(b);
      b = next;
    }
  }
  free(set->buckets);
  free(set);
}

int hashset_size(hashset_t *set)
{
  int size = 0;
  unsigned int i;
  bucket_t *b;

  for (i = 0; i < NB_BUCKETS; i++) {
    b = set->buckets[i];
    while (b != NULL) {
      size++;
      b = b->next;
    }
  }

  return size;
}

int hashset_contains(hashset_t *set, val_t val, thread_data_t *td)
{
  int result, i;
  bucket_t *b;

# ifdef DEBUG
  printf("++> set_contains(%d)\n", val);
  IO_FLUSH;
# endif

  if (!td) {
    i = HASH(val);
    b = set->buckets[i];
    result = 0;
    while (b != NULL) {
      if (b->val == val) {
        result = 1;
        break;
      }
      b = b->next;
    }
  } else {
#ifdef HW_SW_PATHS
		IF_HTM_MODE
			START_HTM_MODE
    		i = HASH(val);
    		b = (bucket_t *)set->buckets[i];
    		result = 0;
    		while (b != NULL) {
      		if (b->val == val) {
        		result = 1;
        		break;
      		}
      		b = (bucket_t *)b->next;
    		}
			COMMIT_HTM_MODE
		ELSE_STM_MODE
			START_STM_MODE(td->threadId,RO)
#else /* !HW_SW_PATHS */
	    TM_START(0, RO);
#endif /* !HW_SW_PATHS */
    		i = HASH(val);
    		b = (bucket_t *)TM_LOAD(&set->buckets[i]);
    		result = 0;
		    while (b != NULL) {
		      if (TM_LOAD(&b->val) == val) {
		        result = 1;
		        break;
		      }
		      b = (bucket_t *)TM_LOAD(&b->next);
		    }
#ifdef HW_SW_PATHS
			COMMIT_STM_MODE
#else /* !HW_SW_PATHS */
    	TM_COMMIT;
#endif /* !HW_SW_PATHS */
  }

  return result;
}

int hashset_add(hashset_t *set, val_t val, thread_data_t *td)
{
  int result, i;
  bucket_t *b, *first;

# ifdef DEBUG
  printf("++> set_add(%d)\n", val);
  IO_FLUSH;
# endif

  if (!td) {
    i = HASH(val);
    first = b = set->buckets[i];
    result = 1;
    while (b != NULL) {
      if (b->val == val) {
        result = 0;
        break;
      }
      b = b->next;
    }
    if (result) {
      set->buckets[i] = new_entry(val, first, 0);
    }
  } else {
#ifdef HW_SW_PATHS
		IF_HTM_MODE
			START_HTM_MODE
		    i = HASH(val);
		    first = b = (bucket_t *)set->buckets[i];
		    result = 1;
		    while (b != NULL) {
		      if (b->val == val) {
		        result = 0;
		        break;
		      }
		      b = (bucket_t *)b->next;
		    }
		    if (result) {
		      set->buckets[i] = new_entry(val, first, 0);
		    }
			COMMIT_HTM_MODE
		ELSE_STM_MODE
			START_STM_MODE(td->threadId,RW)
#else /* !HW_SW_PATHS */
	    TM_START(1, RW);
#endif /* !HW_SW_PATHS */
		    i = HASH(val);
		    first = b = (bucket_t *)TM_LOAD(&set->buckets[i]);
		    result = 1;
		    while (b != NULL) {
		      if (TM_LOAD(&b->val) == val) {
		        result = 0;
		        break;
		      }
		      b = (bucket_t *)TM_LOAD(&b->next);
		    }
		    if (result) {
		      TM_STORE(&set->buckets[i], new_entry(val, first, 1));
		    }
#ifdef HW_SW_PATHS
			COMMIT_STM_MODE
#else /* !HW_SW_PATHS */
    	TM_COMMIT;
#endif /* !HW_SW_PATHS */
  }

  return result;
}

int hashset_remove(hashset_t *set, val_t val, thread_data_t *td)
{
  int result, i;
  bucket_t *b, *prev;

# ifdef DEBUG
  printf("++> set_remove(%d)\n", val);
  IO_FLUSH;
# endif

  if (!td) {
    i = HASH(val);
    prev = b = set->buckets[i];
    result = 0;
    while (b != NULL) {
      if (b->val == val) {
        result = 1;
        break;
      }
      prev = b;
      b = b->next;
    }
    if (result) {
      if (prev == b) {
        /* First element of bucket */
        set->buckets[i] = b->next;
      } else {
        prev->next = b->next;
      }
      free(b);
    }
  } else {
#ifdef HW_SW_PATHS
		IF_HTM_MODE
			START_HTM_MODE
		    i = HASH(val);
		    prev = b = (bucket_t *)set->buckets[i];
		    result = 0;
		    while (b != NULL) {
		      if (b->val == val) {
		        result = 1;
		        break;
		      }
		      prev = b;
		      b = (bucket_t *)b->next;
		    }
		    if (result) {
		      if (prev == b) {
		        /* First element of bucket */
		        set->buckets[i] = b->next;
		      } else {
		        prev->next = b->next;
		      }
		      /* Free memory (delayed until commit) */
		      free(b);
		    }
			COMMIT_HTM_MODE
		ELSE_STM_MODE
			START_STM_MODE(td->threadId,RW)
#else /* !HW_SW_PATHS */
	    TM_START(2, RW);
#endif /* !HW_SW_PATHS */
		    i = HASH(val);
		    prev = b = (bucket_t *)TM_LOAD(&set->buckets[i]);
		    result = 0;
		    while (b != NULL) {
		      if (TM_LOAD(&b->val) == val) {
		        result = 1;
		        break;
		      }
		      prev = b;
		      b = (bucket_t *)TM_LOAD(&b->next);
		    }
		    if (result) {
		      if (prev == b) {
		        /* First element of bucket */
		        TM_STORE(&set->buckets[i], TM_LOAD(&b->next));
		      } else {
		        TM_STORE(&prev->next, TM_LOAD(&b->next));
		      }
		      /* Free memory (delayed until commit) */
		      TM_FREE2(b, sizeof(bucket_t));
		    }
#ifdef HW_SW_PATHS
			COMMIT_STM_MODE
#else /* !HW_SW_PATHS */
    	TM_COMMIT;
#endif /* !HW_SW_PATHS */
  }

  return result;
}
