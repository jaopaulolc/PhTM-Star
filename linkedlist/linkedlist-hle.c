#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#include "linkedlist.h"

#include <hle.h>

hle_lock_t global_lock = HLE_LOCK_INITIALIZER;


typedef struct _list_node_t {
  int key;
  struct _list_node_t *next;
} list_node_t;



void list_init(list_node_t **head)
{
  *head = (list_node_t *)malloc(sizeof(list_node_t));
  (*head)->key = INT_MIN;
  (*head)->next = (list_node_t *)malloc(sizeof(list_node_t));
  (*head)->next->key = INT_MAX;
  (*head)->next->next = NULL;
}

size_t list_size(list_node_t *head)
{
  int count = 0;
  while (head != NULL) {
    count++;
    head = head->next;
  }

  return count;
}

int list_add(list_node_t *head, int item)
{
  list_node_t *pred, *curr;

  list_node_t *node = (list_node_t *)malloc(sizeof(list_node_t));
  hle_lock(&global_lock);
    pred = head;
    curr = head->next;
    while (curr->key < item) {
      pred = curr;
      curr = curr->next;
    }
    
    //list_node_t *node = (list_node_t *)malloc(sizeof(list_node_t));
    node->key = item;
    node->next = curr;
    pred->next = node;
  hle_unlock(&global_lock);
  
  return 1;
}


int list_remove(list_node_t *head, int item) 
{
  list_node_t *pred, *curr;

  hle_lock(&global_lock);
    pred = head;
    curr = head->next;
    while (curr->key < item) {
      pred = curr;
      curr = curr->next;
    }
    if (item == curr->key) { /* found */
      pred->next = curr->next;
      hle_unlock(&global_lock);
      free(curr);
      return 1;
    }
  hle_unlock(&global_lock);
    return 0;
}

int list_contain(list_node_t *head, int item)
{
  list_node_t *curr;
  int found;

  hle_lock(&global_lock);
    curr = head->next;
    while (curr->key < item) {
      curr = curr->next;
    }
    found = (item == curr->key);
  hle_unlock(&global_lock);

  return found;
}




void list_print(list_node_t *head)
{
  if (head == NULL) return;

  list_node_t *curr = head;
  fprintf(stdout, "[%d, ", curr->key);
  curr = curr->next;
  while (curr->next != NULL)
  {
    fprintf(stdout, "%d, ", curr->key);
    curr = curr->next;
  }
  fprintf(stdout, "%d]\n", curr->key);
}



#ifdef TEST_LIST
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

list_node_t *list_head;

int main()
{
  list_init(&list_head);

  list_add(list_head, 0);
  list_add(list_head, -10);
  list_add(list_head, 10);

  assert(list_contain(list_head, 10));
  assert(list_contain(list_head, 0));
  assert(list_contain(list_head, -10));
  
  list_print(list_head);

  list_remove(list_head, 0);
  assert(!list_contain(list_head, 0));
  list_print(list_head);
  

  list_remove(list_head, 10);
  assert(!list_contain(list_head, 10));
  list_print(list_head);

  list_remove(list_head, -10);
  assert(!list_contain(list_head, -10));
  list_print(list_head);

  assert(list_contain(list_head, 0));

  return 0;
}
#endif
