#include <stdio.h>
#include <stdlib.h>
#include "sys/time.h"
#include <unistd.h>

// data structure to operate memory
struct link_node{
  struct link_node* next;
  struct link_node* last;
  int isfree;
  unsigned long length;
  int curr_size;
};

// NODE_SIZE, used in main c program to represent the size(in bytes) of the struct
#define NODE_SIZE sizeof(struct link_node)

// used to set the properties of a new memory chunk
void set_new_malloc(struct link_node* node_ptr, size_t size);

// used to split the free memory into two parts
void split(struct link_node* node, size_t size);



// lock version Best Fit malloc/free
void * ts_malloc_lock(size_t size);
void ts_free_lock(void * ptr);


// no lock version Best Fit malloc/free
void * ts_malloc_nolock(size_t size);
void ts_free_nolock(void * ptr);
