#ifndef TW_TYPES_H
#define TW_TYPES_H

#include <ucontext.h>

typedef unsigned int worker_t;

#define SCHED 0
#define READY 1
#define WAITING 2
#define TERMINATED 3



typedef struct TCB
{
    /* add important states in a thread control block */
    // thread Id
    worker_t tid;
    // thread status
    int status;
    // thread context
    ucontext_t *context;
    int retval; //always init to 0

    // thread stack
    // thread priority
    // And more ...

    // YOUR CODE HERE

}tcb;

#endif
