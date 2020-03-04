# include "my_malloc.h"
# include "assert.h"
# define INT_MAX 2147483647

// global variables in main C program
// convenient for functions to invoke
int first_call = 1;
struct link_node* first_node = NULL;
struct link_node* last_node = NULL;
unsigned long total_free = 0;
unsigned long total_segment = 0;
unsigned long largest_free = 0;

// using glaobal variable total_free to get the current total free memory
unsigned long get_total_free_size(){ 
  return total_free;
}

// using glaobal variable total_segment to get the current total memory (for testing) 
unsigned long get_data_segment_size(){
  return total_segment;
}

// using "once" traverse for Link_Node structure to fidn the largest free data size
unsigned long get_largest_free_data_segment_size(){
  struct link_node * temp= first_node;
  unsigned long curr_max = 0;
  while(temp != NULL){
    if(temp->isfree == 1 && temp->length > curr_max){
      curr_max = temp->length; 
    }
    temp = temp -> next;
  }
  return curr_max;
}

// abstraction both for ff and bf, using it to set the new node's property
// no return, just setting
void set_new_malloc(struct link_node * node_ptr, size_t size){
  node_ptr->last = last_node; // set head
  last_node = node_ptr;
  node_ptr->next = NULL; // set tail
  node_ptr->isfree = 0;
  node_ptr->length = size;
  node_ptr->curr_size = size;
  total_segment += node_ptr->length;
}

// abstraction both for ff and bf, using it to use left space later
// no return, add one node
void split(struct link_node * node, size_t size){
  struct link_node * third = node->next;
  struct link_node * second = (void*)(node + 1) + size;
  node->next = second;
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
  struct link_node* p = (struct link_node*)ptr - 1;
  if (p->isfree == 1){
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


// search the best free node to free, similar with FF, but have to traverse once
// return the node which has the minimized size comparing with the size provided
struct link_node * bf_search_free_node(size_t size){
  struct link_node * temp = first_node;
  struct link_node * best_node = NULL;
  int min = INT_MAX;
  while(temp != NULL){
    if(temp->isfree == 1 && temp->length >= size){
      if (temp->length == size){
	best_node = temp;
	break;
      }
      if (temp->length - size < min){
	min = temp->length;
	best_node = temp;
      }
    }
    temp = temp -> next;
  }
  // to control whether spliting in BF
  if (best_node != NULL){
    best_node->isfree = 0;
    best_node->curr_size = size;
    // assert
    assert(total_free >= size); 
    total_free -= size;//best_node->length;
    // split funciton
    // /*
    if (best_node->length - size > NODE_SIZE){
      split(best_node, size);
    }
   // */
  }
  return best_node;
}

// main API implementation for BF stategy
void * ts_malloc_lock(size_t size){
  struct link_node* node_ptr;
  if (size <= 0){
    return NULL;
  }

  if (first_call){
    node_ptr = sbrk(0); 
    void * malloced = sbrk(size + NODE_SIZE);
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
       node_ptr = sbrk(0); 
       void * malloced = sbrk(size + NODE_SIZE); 
       if (malloced == (void *) -1){
	 return NULL;
       }
       last_node->next = node_ptr;
       set_new_malloc(node_ptr, size);
     }
  }    
  void * content = node_ptr + 1;
  return content;
}
