#ifndef TW_TYPES_H
#define TW_TYPES_H

#include <ucontext.h>

typedef unsigned int worker_t;

#define READY 0
#define RUNNING 1
#define BLOCKED 2

typedef struct TCB
{
    /* add important states in a thread control block */
    // thread Id
    worker_t tid;
    // thread status
    int status;
    // thread context
    ucontext_t *context;
    // thread stack
    // thread priority
    // And more ...

    // YOUR CODE HERE

} tcb;

#endif
