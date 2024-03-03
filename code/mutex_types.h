#ifndef MTX_TYPES_H
#define MTX_TYPES_H

#include "thread_worker_types.h"

/* mutex struct definition */
//node struct for queues
typedef struct node {
	struct node *next;
	tcb *data;
} node;

//queue struct
typedef struct queue {
	struct node *head;
	struct node *tail;
	int size;
} queue;



typedef struct worker_mutex_t
{
    /* add something here */
    int mid;
    int mut; //only accessable through t&s or release
    node* currentUser;
    queue *wait_Q; //waitQ of threads waiting for this mutex

    // YOUR CODE HERE
} worker_mutex_t;

#endif
