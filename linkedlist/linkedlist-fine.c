#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "linkedlist.h"

typedef struct _list_node_t {
  int key;
  pthread_mutex_t lock;
  struct _list_node_t *next;
} list_node_t;



void list_init(list_node_t **head)
{
  *head = (list_node_t *)malloc(sizeof(list_node_t));
  (*head)->key = INT_MIN;
  (*head)->next = (list_node_t *)malloc(sizeof(list_node_t));
  (*head)->next->key = INT_MAX;
  (*head)->next->next = NULL;
  pthread_mutex_init(&(*head)->lock,NULL);
  pthread_mutex_init(&(*head)->next->lock,NULL);
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

  pthread_mutex_lock(&head->lock);

  pred = head;
  curr = pred->next;
  pthread_mutex_lock(&curr->lock);

  while (curr->key < item) {
    pthread_mutex_unlock(&pred->lock);
    pred = curr;
    curr = curr->next;
    pthread_mutex_lock(&curr->lock);
  }
    
  list_node_t *node = (list_node_t *)malloc(sizeof(list_node_t));
  node->key = item;
  pthread_mutex_init(&node->lock,NULL);
  node->next = curr;
  pred->next = node;
  
  pthread_mutex_unlock(&curr->lock);
  pthread_mutex_unlock(&pred->lock);

  return 1;
}


int list_remove(list_node_t *head, int item) 
{
  list_node_t *pred, *curr;

  pthread_mutex_lock(&head->lock);

  pred = head;
  curr = head->next;
  pthread_mutex_lock(&curr->lock);

  while (curr->key < item) {
    pthread_mutex_unlock(&pred->lock);
    pred = curr;
    curr = curr->next;
    pthread_mutex_lock(&curr->lock);
  }
    
  if (item == curr->key) { /* found */
    pred->next = curr->next;
    pthread_mutex_destroy(&curr->lock);
    free(curr);
    pthread_mutex_unlock(&pred->lock);
    return 1;
  }
  
  pthread_mutex_unlock(&curr->lock);
  pthread_mutex_unlock(&pred->lock);
  return 0;
}


int list_contain(list_node_t *head, int item)
{
  list_node_t *pred, *curr;
  int result;
  
  pthread_mutex_lock(&head->lock);

  pred = head;
  curr = pred->next;
  pthread_mutex_lock(&curr->lock);
  while (curr->key < item) {
    pthread_mutex_unlock(&pred->lock);
    pred = curr;
    curr = curr->next;
    pthread_mutex_lock(&curr->lock);
  }
    
  result = item == curr->key;
  
  pthread_mutex_unlock(&curr->lock);
  pthread_mutex_unlock(&pred->lock);
  
  return result;
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
