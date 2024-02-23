#ifndef TW_TYPES_H
#define TW_TYPES_H

#include <ucontext.h>

typedef unsigned int worker_t;

#define SCHED 0
#define READY 1
#define WAITING 2
#define TERMINATED 3
typedef struct joinNode {
	struct node *next;
	worker_t *data;
} joinNode; 

typedef struct TCB
{
    /* add important states in a thread control block */
    // thread Id
    worker_t tid;
    // thread status
    int status;
    // thread context
    ucontext_t *context;
    int retval;
    node* headOfJoiningQ;

    // thread stack
    // thread priority
    // And more ...

    // YOUR CODE HERE

}tcb;

#endif
