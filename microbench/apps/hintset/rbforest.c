#include <stdio.h>
#include <stdlib.h>
#include <rbtree.h>

extern void rbtree_free(rbtree_t* r);
extern int rbtree_insert (rbtree_t* r, void* key, void* val);
extern int rbtree_delete (rbtree_t* r, void* key);
extern int rbtree_contains (rbtree_t* r, void* key);
extern int TMrbtree_insert (rbtree_t* r, void* key, void* val);
extern int TMrbtree_delete (rbtree_t* r, void* key);
extern int TMrbtree_contains (rbtree_t* r, void* key);

struct forest {
  rbtree_t **trees;
	size_t nb_trees;
};

forest_t *forest_new(size_t nb_trees)
{
  forest_t *forest = (forest_t*)malloc(sizeof(forest_t));
	if (forest == NULL) {
		perror("malloc");
		exit(1);
	}
  
	if ((forest->trees = (rbtree_t **)malloc(sizeof(rbtree_t*) * nb_trees)) == NULL) {
    perror("malloc");
   	exit(1);
  }

	size_t i;
	for (i = 0; i < nb_trees; i++) {
		forest->trees[i] = rbtreeset_new();
	}

	forest->nb_trees = nb_trees;

  return forest;
}

void forest_delete(forest_t* forest)
{
	size_t i;
	size_t nb_trees = forest->nb_trees;
	
	for (i = 0; i < nb_trees; i++) {
		rbtree_free(forest->trees[i]);
	}
	free(forest->trees);
  free(forest);
}

int forest_size(forest_t* forest)
{
  int size = 0;
	
	size_t i;
	size_t nb_trees = forest->nb_trees;

	for (i = 0; i < nb_trees; i++) {
		size += rbtreeset_size(forest->trees[i]);
	}

  return size;
}

size_t forest_nb_trees(forest_t* forest) {
	return forest->nb_trees;
}

int forest_contains(forest_t* forest, val_t val, size_t k, thread_data_t* td)
{
  int result = 1;

# ifdef DEBUG
  printf("++> forest_contains(%ld)\n", val);
  IO_FLUSH;
# endif

  if (td == NULL) {
		size_t i;
		for (i = 0; i < k; i++) {
			result &= rbtree_contains(forest->trees[i], (void *)val);
		}
  } else /*if (td->unit_tx == 0)*/ {
#ifdef HW_SW_PATHS
		IF_HTM_MODE
			START_HTM_MODE
				size_t i;
				for (i = 0; i < k; i++) {
					result &= rbtree_contains(forest->trees[i], (void *)val);
				}
			COMMIT_HTM_MODE
		ELSE_STM_MODE
			START_STM_MODE(td->threadId, RO)
				size_t i;
				for (i = 0; i < k; i++) {
					result &= TMrbtree_contains(forest->trees[i], (void *)val);
				}
			COMMIT_STM_MODE
#else /* !HW_SW_PATHS */
    	TM_START(0, RO);
				size_t i;
				for (i = 0; i < k; i++) {
					result &= TMrbtree_contains(forest->trees[i], (void *)val);
				}
    	TM_COMMIT;
#endif /* !HW_SW_PATHS */
  } 
  return result;
}

int forest_add(forest_t* forest, val_t val, size_t k, thread_data_t* td)
{
  int result = 1;

# ifdef DEBUG
  printf("++> forest_add(%ld)\n", val);
  IO_FLUSH;
# endif

  if (td == NULL) {
		size_t i;
		for (i = 0; i < k; i++) {
			result &= rbtree_insert(forest->trees[i], (void *)val, (void*)val);
		}
  } else {
#ifdef HW_SW_PATHS
		IF_HTM_MODE
			START_HTM_MODE
				size_t i;
				for (i = 0; i < k; i++) {
					result &= rbtree_insert(forest->trees[i], (void *)val, (void*)val);
				}
			COMMIT_HTM_MODE
		ELSE_STM_MODE
			START_STM_MODE(td->threadId, RW)
				size_t i;
				for (i = 0; i < k; i++) {
					result &= TMrbtree_insert(forest->trees[i], (void *)val, (void*)val);
				}
			COMMIT_STM_MODE
#else /* !HW_SW_PATHS */
  	  TM_START(1, RW);
				size_t i;
				for (i = 0; i < k; i++) {
					result &= TMrbtree_insert(forest->trees[i], (void *)val, (void*)val);
				}
    	TM_COMMIT;
#endif /* !HW_SW_PATHS */
  } 
  return result;
}

int forest_remove(forest_t* forest, val_t val, size_t k, thread_data_t* td)
{
  int result = 1;

# ifdef DEBUG
  printf("++> forest_remove(%ld)\n", val);
  IO_FLUSH;
# endif

  if (td == NULL) {
		size_t i;
		for (i = 0; i < k; i++) {
			result &= rbtree_delete(forest->trees[i], (void *)val);
		}
  } else {
#ifdef HW_SW_PATHS
		IF_HTM_MODE
			START_HTM_MODE
				size_t i;
				for (i = 0; i < k; i++) {
					result &= rbtree_delete(forest->trees[i], (void *)val);
				}
			COMMIT_HTM_MODE
		ELSE_STM_MODE
			START_STM_MODE(td->threadId, RW)
				size_t i;
				for (i = 0; i < k; i++) {
					result &= TMrbtree_delete(forest->trees[i], (void *)val);
				}
			COMMIT_STM_MODE
#else /* !HW_SW_PATHS */
	    TM_START(2, RW);
				size_t i;
				for (i = 0; i < k; i++) {
					result &= TMrbtree_delete(forest->trees[i], (void *)val);
				}
    	TM_COMMIT;
#endif /* !HW_SW_PATHS */
  }
  return result;
}
