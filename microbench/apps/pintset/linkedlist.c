/* ################################################################### *
 * LINKEDLIST
 * ################################################################### */

#include <stdio.h>
#include <stdlib.h>
#include <linkedlist.h>

# define INIT_SET_PARAMETERS            /* Nothing */

typedef struct node {
  val_t val;
  struct node *next;
} node_t;

struct llistset {
  node_t *head;
};

TM_SAFE
static node_t *new_node(val_t val, node_t *next, int transactional)
{
  node_t *node;

  if (!transactional) {
    node = (node_t *)malloc(sizeof(node_t));
  } else {
    node = (node_t *)TM_MALLOC(sizeof(node_t));
  }
  if (node == NULL) {
    perror("malloc");
    exit(1);
  }

  node->val = val;
  node->next = next;

  return node;
}

llistset_t *llistset_new()
{
  llistset_t *set;
  node_t *min, *max;

  if ((set = (llistset_t *)malloc(sizeof(llistset_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  max = new_node(VAL_MAX, NULL, 0);
  min = new_node(VAL_MIN, max, 0);
  set->head = min;

  return set;
}

void llistset_delete(llistset_t *set)
{
  node_t *node, *next;

  node = set->head;
  while (node != NULL) {
    next = node->next;
    free(node);
    node = next;
  }
  free(set);
}

int llistset_size(llistset_t *set)
{
  int size = 0;
  node_t *node;

  /* We have at least 2 elements */
  node = set->head->next;
  while (node->next != NULL) {
    size++;
    node = node->next;
  }

  return size;
}

int llistset_contains(llistset_t *set, val_t val, thread_data_t *td)
{
  int result;
  node_t *prev, *next;
  val_t v;

# ifdef DEBUG
  printf("++> set_contains(%d)\n", val);
  IO_FLUSH;
# endif

  if (td == NULL) {
    prev = set->head;
    next = prev->next;
    while (next->val < val) {
      prev = next;
      next = prev->next;
    }
    result = (next->val == val);
  } else /*if (td->unit_tx == 0)*/ {
#ifdef HW_SW_PATHS
		IF_HTM_MODE
			START_HTM_MODE
		    prev = set->head;
		    next = prev->next;
		    while (next->val < val) {
		      prev = next;
		      next = prev->next;
		    }
		    result = (next->val == val);
			COMMIT_HTM_MODE
		ELSE_STM_MODE
			START_STM_MODE(td->threadId, RO)
#else /* !HW_SW_PATHS */
    	TM_START(0, RO);
#endif /* !HW_SW_PATHS */
		    prev = (node_t *)TM_LOAD(&set->head);
		    next = (node_t *)TM_LOAD(&prev->next);
		    while (1) {
		      v = TM_LOAD(&next->val);
		      if (v >= val)
		        break;
		      prev = next;
		      next = (node_t *)TM_LOAD(&prev->next);
		    }
		    result = (v == val);
#ifdef HW_SW_PATHS
			COMMIT_STM_MODE
#else /* !HW_SW_PATHS */
    	TM_COMMIT;
#endif /* !HW_SW_PATHS */
  } 
  return result;
}

int llistset_add(llistset_t *set, val_t val, thread_data_t *td)
{
  int result;
  node_t *prev, *next;
  val_t v;

# ifdef DEBUG
  printf("++> set_add(%d)\n", val);
  IO_FLUSH;
# endif

  if (td == NULL) {
    prev = set->head;
    next = prev->next;
    while (next->val < val) {
      prev = next;
      next = prev->next;
    }
    result = (next->val != val);
    if (result) {
      prev->next = new_node(val, next, 0);
    }
  } else /*if (td->unit_tx == 0)*/ {
#ifdef HW_SW_PATHS
		IF_HTM_MODE
			START_HTM_MODE
		    prev = set->head;
		    next = prev->next;
		    while (next->val < val) {
		      prev = next;
		      next = prev->next;
		    }
		    result = (next->val != val);
		    if (result) {
		      prev->next = new_node(val, next, 0);
		    }
			COMMIT_HTM_MODE
		ELSE_STM_MODE
			START_STM_MODE(td->threadId, RW)
#else /* !HW_SW_PATHS */
  	  TM_START(1, RW);
#endif /* !HW_SW_PATHS */
		    prev = (node_t *)TM_LOAD(&set->head);
		    next = (node_t *)TM_LOAD(&prev->next);
		    while (1) {
		      v = TM_LOAD(&next->val);
		      if (v >= val)
		        break;
		      prev = next;
		      next = (node_t *)TM_LOAD(&prev->next);
		    }
		    result = (v != val);
		    if (result) {
		      TM_STORE(&prev->next, new_node(val, next, 1));
		    }
#ifdef HW_SW_PATHS
			COMMIT_STM_MODE
#else /* !HW_SW_PATHS */
    	TM_COMMIT;
#endif /* !HW_SW_PATHS */
  } 
  return result;
}

int llistset_remove(llistset_t *set, val_t val, thread_data_t *td)
{
  int result;
  node_t *prev, *next;
  val_t v;
  node_t *n;

# ifdef DEBUG
  printf("++> set_remove(%d)\n", val);
  IO_FLUSH;
# endif

  if (td == NULL) {
    prev = set->head;
    next = prev->next;
    while (next->val < val) {
      prev = next;
      next = prev->next;
    }
    result = (next->val == val);
    if (result) {
      prev->next = next->next;
      free(next);
    }
  } else {
#ifdef HW_SW_PATHS
		IF_HTM_MODE
			START_HTM_MODE
		    prev = set->head;
		    next = prev->next;
		    while (next->val < val) {
		      prev = next;
		      next = prev->next;
		    }
		    result = (next->val == val);
		    if (result) {
		      prev->next = next->next;
		      free(next);
		    }
			COMMIT_HTM_MODE
		ELSE_STM_MODE
			START_STM_MODE(td->threadId, RW)
#else /* !HW_SW_PATHS */
	    TM_START(2, RW);
#endif /* !HW_SW_PATHS */
		    prev = (node_t *)TM_LOAD(&set->head);
		    next = (node_t *)TM_LOAD(&prev->next);
		    while (1) {
		      v = TM_LOAD(&next->val);
		      if (v >= val)
		        break;
		      prev = next;
		      next = (node_t *)TM_LOAD(&prev->next);
		    }
		    result = (v == val);
		    if (result) {
		      n = (node_t *)TM_LOAD(&next->next);
		      TM_STORE(&prev->next, n);
		      /* Free memory (delayed until commit) */
		      TM_FREE2(next, sizeof(node_t));
		    }
#ifdef HW_SW_PATHS
			COMMIT_STM_MODE
#else /* !HW_SW_PATHS */
    	TM_COMMIT;
#endif /* !HW_SW_PATHS */
  }
  return result;
}
