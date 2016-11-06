/* ################################################################### *
 * SKIPLIST
 * ################################################################### */

#include <stdio.h>
#include <stdlib.h>
#include <skiplist.h>

extern unsigned short main_seed[3];

# define MAX_LEVEL                      64

# define INIT_MAX_LEVEL                 32
# define INIT_PROB                      50

typedef intptr_t val_t;
typedef intptr_t level_t;
# define VAL_MIN                        INT_MIN
# define VAL_MAX                        INT_MAX

typedef struct node {
  val_t val;
  level_t level;
  struct node *forward[1];
} node_t;

struct slistset {
  node_t *head;
  node_t *tail;
  level_t level;
  int prob;
  int max_level;
};

TM_PURE
static int random_level(slistset_t *set, unsigned short *seed)
{
  int l = 0;
  while (l < set->max_level && rand_range(100, seed) < set->prob)
    l++;
  return l;
}

TM_SAFE
static node_t *new_node(val_t val, level_t level, int transactional)
{
  node_t *node;

  if (!transactional) {
    node = (node_t *)malloc(sizeof(node_t) + level * sizeof(node_t *));
  } else {
    node = (node_t *)TM_MALLOC(sizeof(node_t) + level * sizeof(node_t *));
  }
  if (node == NULL) {
    perror("malloc");
    exit(1);
  }

  node->val = val;
  node->level = level;

  return node;
}

slistset_t *slistset_new()
{
	level_t max_level = INIT_MAX_LEVEL;
	int prob = INIT_PROB;
  slistset_t *set;
  int i;

  assert(max_level <= MAX_LEVEL);
  assert(prob >= 0 && prob <= 100);

  if ((set = (slistset_t *)malloc(sizeof(slistset_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  set->max_level = max_level;
  set->prob = prob;
  set->level = 0;
  /* Set head and tail are immutable */
  set->tail = new_node(VAL_MAX, max_level, 0);
  set->head = new_node(VAL_MIN, max_level, 0);
  for (i = 0; i <= max_level; i++) {
    set->head->forward[i] = set->tail;
    set->tail->forward[i] = NULL;
  }

  return set;
}

void slistset_delete(slistset_t *set)
{
  node_t *node, *next;

  node = set->head;
  while (node != NULL) {
    next = node->forward[0];
    free(node);
    node = next;
  }
  free(set);
}

int slistset_size(slistset_t *set)
{
  int size = 0;
  node_t *node;

  /* We have at least 2 elements */
  node = set->head->forward[0];
  while (node->forward[0] != NULL) {
    size++;
    node = node->forward[0];
  }

  return size;
}

int slistset_contains(slistset_t *set, val_t val, thread_data_t *td)
{
  int result, i;
  node_t *node, *next;
  val_t v;

# ifdef DEBUG
  printf("++> slistset_contains(%d)\n", val);
  IO_FLUSH;
# endif

  if (!td) {
    node = set->head;
    for (i = set->level; i >= 0; i--) {
      next = node->forward[i];
      while (next->val < val) {
        node = next;
        next = node->forward[i];
      }
    }
    node = node->forward[0];
    result = (node->val == val);
  } else {
#ifdef HW_SW_PATHS
		IF_HTM_MODE
			START_HTM_MODE
		    v = VAL_MIN; /* Avoid compiler warning (should not be necessary) */
		    node = set->head;
		    for (i = set->level; i >= 0; i--) {
		      next = (node_t *)node->forward[i];
		      while (1) {
		        v = next->val;
		        if (v >= val)
		          break;
		        node = next;
		        next = (node_t *)node->forward[i];
		      }
		    }
		    result = (v == val);
			COMMIT_HTM_MODE
		ELSE_STM_MODE
			START_STM_MODE(td->threadId, RO)
		    v = VAL_MIN; /* Avoid compiler warning (should not be necessary) */
		    node = set->head;
		    for (i = TM_LOAD(&set->level); i >= 0; i--) {
		      next = (node_t *)TM_LOAD(&node->forward[i]);
		      while (1) {
		        v = TM_LOAD(&next->val);
		        if (v >= val)
		          break;
		        node = next;
		        next = (node_t *)TM_LOAD(&node->forward[i]);
		      }
		    }
		    result = (v == val);
			COMMIT_STM_MODE
#else /* !HW_SW_PATHS */
    TM_START(0, RO);
    v = VAL_MIN; /* Avoid compiler warning (should not be necessary) */
    node = set->head;
    for (i = TM_LOAD(&set->level); i >= 0; i--) {
      next = (node_t *)TM_LOAD(&node->forward[i]);
      while (1) {
        v = TM_LOAD(&next->val);
        if (v >= val)
          break;
        node = next;
        next = (node_t *)TM_LOAD(&node->forward[i]);
      }
    }
    result = (v == val);
    TM_COMMIT;
#endif /* !HW_SW_PATHS */
  }

  return result;
}

int slistset_add(slistset_t *set, val_t val, thread_data_t *td)
{
  int result, i;
  node_t *update[MAX_LEVEL + 1];
  node_t *node, *next;
  level_t level, l;
  val_t v;

# ifdef DEBUG
  printf("++> slistset_add(%d)\n", val);
  IO_FLUSH;
# endif

  if (!td) {
    node = set->head;
    for (i = set->level; i >= 0; i--) {
      next = node->forward[i];
      while (next->val < val) {
        node = next;
        next = node->forward[i];
      }
      update[i] = node;
    }
    node = node->forward[0];

    if (node->val == val) {
      result = 0;
    } else {
      l = random_level(set, main_seed);
      if (l > set->level) {
        for (i = set->level + 1; i <= l; i++)
          update[i] = set->head;
        set->level = l;
      }
      node = new_node(val, l, 0);
      for (i = 0; i <= l; i++) {
        node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = node;
      }
      result = 1;
    }
  } else {
#ifdef HW_SW_PATHS
		IF_HTM_MODE
			START_HTM_MODE
		    v = VAL_MIN; /* Avoid compiler warning (should not be necessary) */
		    node = set->head;
		    level = set->level;
		    for (i = level; i >= 0; i--) {
		      next = (node_t *)node->forward[i];
		      while (1) {
		        v = next->val;
		        if (v >= val)
		          break;
		        node = next;
		        next = (node_t *)node->forward[i];
		      }
		      update[i] = node;
		    }
		
		    if (v == val) {
		      result = 0;
		    } else {
		      l = random_level(set, td->seed);
		      if (l > level) {
		        for (i = level + 1; i <= l; i++)
		          update[i] = set->head;
		        set->level = l;
		      }
		      node = new_node(val, l, 0);
		      for (i = 0; i <= l; i++) {
		        node->forward[i] = (node_t *)update[i]->forward[i];
		        update[i]->forward[i] = node;
		      }
		      result = 1;
		    }
			COMMIT_HTM_MODE
		ELSE_STM_MODE
			START_STM_MODE(td->threadId, RW)
		    v = VAL_MIN; /* Avoid compiler warning (should not be necessary) */
		    node = set->head;
		    level = TM_LOAD(&set->level);
		    for (i = level; i >= 0; i--) {
		      next = (node_t *)TM_LOAD(&node->forward[i]);
		      while (1) {
		        v = TM_LOAD(&next->val);
		        if (v >= val)
		          break;
		        node = next;
		        next = (node_t *)TM_LOAD(&node->forward[i]);
		      }
		      update[i] = node;
		    }
		
		    if (v == val) {
		      result = 0;
		    } else {
		      l = random_level(set, td->seed);
		      if (l > level) {
		        for (i = level + 1; i <= l; i++)
		          update[i] = set->head;
		        TM_STORE(&set->level, l);
		      }
		      node = new_node(val, l, 1);
		      for (i = 0; i <= l; i++) {
		        node->forward[i] = (node_t *)TM_LOAD(&update[i]->forward[i]);
		        TM_STORE(&update[i]->forward[i], node);
		      }
		      result = 1;
		    }
			COMMIT_STM_MODE
#else /* !HW_SW_PATHS */
    TM_START(1, RW);
    v = VAL_MIN; /* Avoid compiler warning (should not be necessary) */
    node = set->head;
    level = TM_LOAD(&set->level);
    for (i = level; i >= 0; i--) {
      next = (node_t *)TM_LOAD(&node->forward[i]);
      while (1) {
        v = TM_LOAD(&next->val);
        if (v >= val)
          break;
        node = next;
        next = (node_t *)TM_LOAD(&node->forward[i]);
      }
      update[i] = node;
    }

    if (v == val) {
      result = 0;
    } else {
      l = random_level(set, td->seed);
      if (l > level) {
        for (i = level + 1; i <= l; i++)
          update[i] = set->head;
        TM_STORE(&set->level, l);
      }
      node = new_node(val, l, 1);
      for (i = 0; i <= l; i++) {
        node->forward[i] = (node_t *)TM_LOAD(&update[i]->forward[i]);
        TM_STORE(&update[i]->forward[i], node);
      }
      result = 1;
    }
    TM_COMMIT;
#endif /* !HW_SW_PATHS */
  }

  return result;
}

int slistset_remove(slistset_t *set, val_t val, thread_data_t *td)
{
  int result, i;
  node_t *update[MAX_LEVEL + 1];
  node_t *node, *next;
  level_t level;
  val_t v;

# ifdef DEBUG
  printf("++> slistset_remove(%d)\n", val);
  IO_FLUSH;
# endif

  if (!td) {
    node = set->head;
    for (i = set->level; i >= 0; i--) {
      next = node->forward[i];
      while (next->val < val) {
        node = next;
        next = node->forward[i];
      }
      update[i] = node;
    }
    node = node->forward[0];

    if (node->val != val) {
      result = 0;
    } else {
      for (i = 0; i <= set->level; i++) {
        if (update[i]->forward[i] == node)
          update[i]->forward[i] = node->forward[i];
      }
      while (set->level > 0 && set->head->forward[set->level]->forward[0] == NULL)
        set->level--;
      free(node);
      result = 1;
    }
  } else {
#ifdef HW_SW_PATHS
		IF_HTM_MODE
			START_HTM_MODE
		    v = VAL_MIN; /* Avoid compiler warning (should not be necessary) */
		    node = set->head;
		    level = set->level;
		    for (i = level; i >= 0; i--) {
		      next = (node_t *)node->forward[i];
		      while (1) {
		        v = next->val;
		        if (v >= val)
		          break;
		        node = next;
		        next = (node_t *)node->forward[i];
		      }
		      update[i] = node;
		    }
		    node = (node_t *)node->forward[0];
		
		    if (v != val) {
		      result = 0;
		    } else {
		      for (i = 0; i <= level; i++) {
		        if ((node_t *)update[i]->forward[i] == node)
		          update[i]->forward[i] = (node_t *)node->forward[i];
		      }
		      i = level;
		      while (i > 0 && (node_t *)set->head->forward[i] == set->tail)
		        i--;
		      if (i != level)
		        set->level = i;
		      /* Free memory (delayed until commit) */
		      free(node);
		      result = 1;
		    }
			COMMIT_HTM_MODE
		ELSE_STM_MODE
			START_STM_MODE(td->threadId, RW)
		    v = VAL_MIN; /* Avoid compiler warning (should not be necessary) */
		    node = set->head;
		    level = TM_LOAD(&set->level);
		    for (i = level; i >= 0; i--) {
		      next = (node_t *)TM_LOAD(&node->forward[i]);
		      while (1) {
		        v = TM_LOAD(&next->val);
		        if (v >= val)
		          break;
		        node = next;
		        next = (node_t *)TM_LOAD(&node->forward[i]);
		      }
		      update[i] = node;
		    }
		    node = (node_t *)TM_LOAD(&node->forward[0]);
		
		    if (v != val) {
		      result = 0;
		    } else {
		      for (i = 0; i <= level; i++) {
		        if ((node_t *)TM_LOAD(&update[i]->forward[i]) == node)
		          TM_STORE(&update[i]->forward[i], (node_t *)TM_LOAD(&node->forward[i]));
		      }
		      i = level;
		      while (i > 0 && (node_t *)TM_LOAD(&set->head->forward[i]) == set->tail)
		        i--;
		      if (i != level)
		        TM_STORE(&set->level, i);
		      /* Free memory (delayed until commit) */
		      TM_FREE2(node, sizeof(node_t) + node->level * sizeof(node_t *));
		      result = 1;
		    }
			COMMIT_STM_MODE
#else /* !HW_SW_PATHS */
    TM_START(2, RW);
    v = VAL_MIN; /* Avoid compiler warning (should not be necessary) */
    node = set->head;
    level = TM_LOAD(&set->level);
    for (i = level; i >= 0; i--) {
      next = (node_t *)TM_LOAD(&node->forward[i]);
      while (1) {
        v = TM_LOAD(&next->val);
        if (v >= val)
          break;
        node = next;
        next = (node_t *)TM_LOAD(&node->forward[i]);
      }
      update[i] = node;
    }
    node = (node_t *)TM_LOAD(&node->forward[0]);

    if (v != val) {
      result = 0;
    } else {
      for (i = 0; i <= level; i++) {
        if ((node_t *)TM_LOAD(&update[i]->forward[i]) == node)
          TM_STORE(&update[i]->forward[i], (node_t *)TM_LOAD(&node->forward[i]));
      }
      i = level;
      while (i > 0 && (node_t *)TM_LOAD(&set->head->forward[i]) == set->tail)
        i--;
      if (i != level)
        TM_STORE(&set->level, i);
      /* Free memory (delayed until commit) */
      TM_FREE2(node, sizeof(node_t) + node->level * sizeof(node_t *));
      result = 1;
    }
    TM_COMMIT;
#endif /* !HW_SW_PATHS */
  }

  return result;
}
