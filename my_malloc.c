# include "my_malloc.h"
# include "assert.h"
# include "pthread.h"
# define INT_MAX 2147483647

// global variables in main C program
// convenient for functions to invoke
/*
static int first_call = 1;
static struct link_node* first_node = NULL;
static struct link_node* last_node = NULL;
static unsigned long total_free = 0;
*/
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
// no lock
int first_call = 1;
struct link_node* first_node = NULL;
struct link_node* last_node = NULL;
unsigned long total_free = 0;
// abstraction both for ff and bf, using it to set the new node's property
// no return, just setting
void set_new_malloc(struct link_node * node_ptr, size_t size){
  node_ptr->last = last_node; // set head
  last_node = node_ptr;
  node_ptr->next = NULL; // set tail
  node_ptr->isfree = 0;
  node_ptr->length = size;
  node_ptr->curr_size = size;
}

// abstraction both for ff and bf, using it to use left space later
// no return, add one node
void split(struct link_node * node, size_t size){
  struct link_node * third = node->next; // the first node after the gap
  struct link_node * second = (void*)(node + 1) + size; // the head addrres of the gap
  node->next = second; // may need to check 
  if (third != NULL){
    second->next = third; 
    third->last = second;
  }
  else{
    second->next = NULL;
  }
  second->last = node; 
  second->isfree = 1;
  second->length = node->length - size - NODE_SIZE;
  second->curr_size = 0;
  assert(total_free >= NODE_SIZE);
  total_free -= NODE_SIZE;
  node->length = size;
}

// free the content by provided pointer
// involve mergeing with adjacent free area
// no return value  
void ts_free_lock(void *ptr){
  if (ptr == NULL){
    return;
  }
  pthread_mutex_lock(&mutex);
  struct link_node* p = (struct link_node*)ptr - 1; // go back to the head address of metanode, may be dangerous!
  if (p == NULL || p->isfree == 1){ 
    pthread_mutex_unlock(&mutex);
    return;
  }
  
  total_free += p->length; // dynamic change
  p->isfree = 1;
  p->curr_size = 0;
  // merge 
  if (p->last != NULL && p->last->isfree == 1){ // last node is a free node
    p = p->last;
    if (p->next->next != NULL && p->next->next->isfree == 1){ // situation 1: 3 adjacent 2,3,4
      //p = p->last;
      p->length = p->length + p->next->length + p->next->next->length + 2 * NODE_SIZE;
      p->next = p->next->next->next;
      total_free += 2 * NODE_SIZE; 
    }
    else{ //situation 2: 2 adjacent 
      p->length = p->length + p->next->length + 1 * NODE_SIZE;
      p->next = p->next->next;
      total_free += NODE_SIZE;
    }

    if(p->next != NULL){
      p->next->last = p;
    }
    pthread_mutex_unlock(&mutex);
    return;
  }

  else if (p->next != NULL && p->next->isfree == 1){ // situation 3: 2 adjacent)
    p->length = p->length + p->next->length + 1 * NODE_SIZE;
    p->next = p->next->next;
    total_free += NODE_SIZE;
    if(p->next != NULL){
      p->next->last = p;
    }
  }

  pthread_mutex_unlock(&mutex);
}


// search the best free node to free, similar with FF, but have to traverse once
// return the node which has the minimized size comparing with the size provided
struct link_node * bf_search_free_node(size_t size){
  struct link_node * temp = first_node;
  struct link_node * best_node = NULL; // best node to malloc in the pool.
  int min = INT_MAX;
  while(temp != NULL){
    if(temp->isfree == 1 && temp->length >= size){ // free area and big enough
      if (temp->length == size){ // equal, just break out of while
	best_node = temp;
	break; 
      }
      if (temp->length - size < min){ // find the smallest gap
	min = temp->length;
	best_node = temp;
      }
    }
    temp = temp -> next; //iteration
  }
  // to control whether spliting in BF
  if (best_node != NULL){ // if we find the best node 
    best_node->isfree = 0;
    best_node->curr_size = size;
    // assert
    assert(total_free >= size); 
    total_free -= size;//best_node->length;
    // split funciton
    // /*
    if (best_node->length - size > NODE_SIZE){ // if we still have gap 
      split(best_node, size);
    }
   // */
  }
  return best_node;
}

// main API implementation for BF stategy
void * ts_malloc_lock(size_t size){
  pthread_mutex_lock(&mutex);
  struct link_node* node_ptr;
  if (size <= 0){
    pthread_mutex_unlock(&mutex);
    return NULL;
  }

  if (first_call){
    node_ptr = sbrk(0); 
    void * malloced = sbrk(size + NODE_SIZE);
    if (malloced == (void *) -1){
      pthread_mutex_unlock(&mutex);
      return NULL;
    }
    
    first_call = 0;
    first_node = node_ptr;
    set_new_malloc(node_ptr, size);
  }
// when malloc is not in the first time
  else{
    // best fit algorithm
     node_ptr = bf_search_free_node(size);
     // find usable space for node
     if (node_ptr == NULL){ // there is no usable space, need new malloc 
       node_ptr = sbrk(0); 
       void * malloced = sbrk(size + NODE_SIZE); // arrange size again
       if (malloced == (void *) -1){
         pthread_mutex_unlock(&mutex);
	 return NULL;
       }
       if (last_node != NULL){ // may be a problem here 
         last_node->next = node_ptr;
       }
       set_new_malloc(node_ptr, size); // fulfill the metanode's information
     }
  }    
  void * content = node_ptr + 1; // add the length of the node
  pthread_mutex_unlock(&mutex);
  return content;
}




//*******************************************************************************



// 
void * ts_malloc_nolock(size_t size){
  
  struct link_node* node_ptr;
  if (size <= 0){
    return NULL;
  }

  if (first_call){
    pthread_mutex_lock(&mutex);
    node_ptr = sbrk(0); 
    void * malloced = sbrk(size + NODE_SIZE);
    pthread_mutex_unlock(&mutex);
    if (malloced == (void *) -1){
      return NULL;
    }
    
    first_call = 0;
    first_node = node_ptr;
    set_new_malloc(node_ptr, size);
  }
// when malloc is not in the first time
  else{  
     node_ptr = bf_search_free_node(size);
     // find usable space for node
     if (node_ptr == NULL){ // there is no usable space, need new malloc 
       pthread_mutex_lock(&mutex);
       node_ptr = sbrk(0); 
       void * malloced = sbrk(size + NODE_SIZE);
       pthread_mutex_unlock(&mutex);
       if (malloced == (void *) -1){
	 return NULL;
       }
       if (last_node != NULL){
         last_node->next = node_ptr;
       }
       set_new_malloc(node_ptr, size);
     }
  }    
  void * content = node_ptr + 1;
  return content;
}


void ts_free_nolock(void *ptr){
  if (ptr == NULL){
    return;
  }
  struct link_node* p = (struct link_node*)ptr - 1;
  if (p == NULL || p->isfree == 1){
    return;
  }
  
  total_free += p->length; // dynamic change
  p->isfree = 1;
  p->curr_size = 0;
  // merge 
  if (p->last != NULL && p->last->isfree == 1){ // last node is a free node
    p = p->last;
    if (p->next->next != NULL && p->next->next->isfree == 1){ // situation 1: 3 adjacent
      //p = p->last;
      p->length = p->length + p->next->length + p->next->next->length + 2 * NODE_SIZE;
      p->next = p->next->next->next;
      total_free += 2 * NODE_SIZE; 
    }
    else{ //situation 2: 2 adjacent
      p->length = p->length + p->next->length + 1 * NODE_SIZE;
      p->next = p->next->next;
      total_free += NODE_SIZE;
    }

    if(p->next != NULL){
      p->next->last = p;
    }
    return;
  }

  else if (p->next != NULL && p->next->isfree == 1){ // situation 3: 2 adjacent)
    p->length = p->length + p->next->length + 1 * NODE_SIZE;
    p->next = p->next->next;
    total_free += NODE_SIZE;
    if(p->next != NULL){
      p->next->last = p;
    }
  }
}


