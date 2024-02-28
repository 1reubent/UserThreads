// File:	thread-worker.c

// List all group member's name:
/*
 */
// username of iLab:
// iLab Server:

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <ucontext.h>
#include "thread-worker.h"
#include "thread_worker_types.h"

#define STACK_SIZE 16 * 1024
#define QUANTUM 10 * 1000


// INITIALIZE ALL YOUR OTHER VARIABLES HERE
int init_scheduler_done = 0;
//int isMainThreadCreated =0;
int currentTID =0; //use as the tid of the next new thread
tcb *schedTCB; //tcb of the scheduler. scheduler doesnt have a node. tid is always -1.
node* currentThread; //node of the currently running thread
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

//only need a runQ and maybe a waitQ, but maybe not.
queue *wait_Q, *run_Q;

//scheduler thread

void signal_handler(int signum){
   swapcontext(currentThread->data->context, schedTCB->context);
}

// timer
struct itimerval timer;
void initSchedulerQsandTimer(){
    //init Qs
    wait_Q->head = NULL;
    wait_Q->tail = NULL;
    wait_Q->size =0;
    run_Q->head = NULL;
    run_Q->tail =NULL;
    run_Q->size =0;

    
    //init schdeuler context
    ucontext_t *schedContext = (ucontext_t*)malloc(sizeof(ucontext_t));
    void *stack=malloc(STACK_SIZE);

    schedContext->uc_link=NULL;
    schedContext->uc_stack.ss_sp=stack;
    schedContext->uc_stack.ss_size=STACK_SIZE;
    schedContext->uc_stack.ss_flags=0;

    makecontext(schedContext,(void *)&schedule,0);
    
    //init scheduler tcb
    schedTCB = (tcb *) malloc(sizeof(tcb));
    schedTCB->context = schedContext;
    schedTCB->status = SCHED;
    schedTCB->tid = -1;
    schedTCB->retval = -1;
    schedTCB->headOfJoiningQ = NULL;

    //init timer signal. schedule() is the signal handler
    struct sigaction sa;
	memset (&sa, 0, sizeof (sa));
	sa.sa_handler = &signal_handler;
	sigaction(SIGPROF, &sa, NULL);
}

//enqueue function
int enqueue (queue *Q, node* thread) {

        //add to the tail of the queue
        if (Q->tail) {
            //enqueue behind tail
            Q->tail->next = thread;
            Q->tail = thread;
        }
        else{
            //empty Q
            Q->head = thread;
            Q->tail = thread;
        }
    (Q->size)++;

    return 0;
}

//FINISH
node* dequeue(queue *Q){
    //dequeue head
    node* removed =NULL;
    if (Q->head) {
        removed =Q->head;
        Q->head= Q->head->next; 
        (Q->size)--;
    }
    return removed;
}

node* removeNode(queue *Q, worker_t toRemove){
    node* ptr = Q->head;
    if( Q->head->data->tid == (int) toRemove){ //if it's the head
        Q->head= Q->head->next;
        return ptr;
    }
    
    
    node* prev = ptr;

    
    while(ptr && (int) ptr->data->tid != (int) toRemove){
        prev = ptr;
        ptr = ptr->next;
    }
    if(ptr!=NULL){
        if( Q->tail->data->tid == (int) toRemove){ //if it's the tail
            prev->next =NULL;
            Q->tail = prev;
            return ptr;
        }
        prev->next = ptr->next;
    }

    return ptr;
}
//FINISH
node* searchQ(queue *Q, worker_t toFind){
    //dont remove from gueu
    node* ptr = Q->head;

    while(ptr && (int) ptr->data->tid != (int) toFind){
        ptr = ptr->next;
    }

    return ptr;
}

void armTimer(){
    timer.it_interval.tv_usec = QUANTUM; 
	timer.it_interval.tv_sec = 0;
	timer.it_value.tv_usec = QUANTUM;
	timer.it_value.tv_sec = 0;
    //start timer
    //setitimer(ITIMER_PROF, &timer, NULL);
}
void disarmTimer(){
    timer.it_interval.tv_usec = 0; 
	timer.it_interval.tv_sec = 0;
	timer.it_value.tv_usec = 0;
	timer.it_value.tv_sec = 0;
    //start timer
    //setitimer(ITIMER_PROF, &timer, NULL);
}
/* create a new thread */
int worker_create(worker_t *thread, pthread_attr_t *attr, void *(*function)(void *), void *arg)
{
    //if first time calling woker_create
    if(init_scheduler_done ==0){
       
        //init scheduler thread, signal handler, + Qs
        initSchedulerQsandTimer();
        
        //initialize main thread
        //init context
        ucontext_t *mainContext = (ucontext_t*)malloc(sizeof(ucontext_t));
        if (getcontext(currentThread->data->context) < 0){//save current context to caller/main thread
            perror("getcontext");
            exit(1);
        }
        mainContext->uc_stack.ss_sp = malloc(STACK_SIZE);
        if (mainContext->uc_stack.ss_sp == NULL){
            perror("Failed to allocate stack");
            exit(1);
        }
        // - allocate space of stack for this thread to run
        mainContext->uc_stack.ss_size = STACK_SIZE;
        mainContext->uc_link = NULL;
        mainContext->uc_stack.ss_flags = 0;
            
        makecontext(mainContext, function, 1, arg);
        //init tcb
        tcb *mainTCB = (tcb *) malloc(sizeof(tcb));
        mainTCB->context = mainContext;
        mainTCB->status = READY;
        mainTCB->tid = currentTID++;
        mainTCB->retval =-1;
        mainTCB->headOfJoiningQ = NULL;

        //init queue node
        node *main = (node *) malloc(sizeof(node));
        main->data = mainTCB;
        main->next = NULL;
        //enqueue main thread
        enqueue(run_Q, main);

        currentThread = main;

    }
    // - create and initialize the context of new worker thread
    
    ucontext_t *newThreadContext = (ucontext_t *) malloc(sizeof(ucontext_t));
    getcontext(newThreadContext);

    newThreadContext->uc_stack.ss_sp = malloc(STACK_SIZE);
    if (newThreadContext->uc_stack.ss_sp == NULL){
		perror("Failed to allocate stack");
		exit(1);
	}
    // - allocate space of stack for this thread to run
	newThreadContext->uc_stack.ss_size = STACK_SIZE;
	newThreadContext->uc_link = NULL;
    newThreadContext->uc_stack.ss_flags = 0;
		
    makecontext(newThreadContext, function, 1, arg);

    // - create new Thread Control Block (TCB)
    tcb *newTCB = (tcb *) malloc(sizeof(tcb));
    newTCB->context = newThreadContext;
    newTCB->status = READY;
    newTCB->tid = currentTID;
    newTCB->retval =-1;
    newTCB->headOfJoiningQ = NULL; 
    //give tid to caller
    *thread = currentTID++;
    
    // after everything is set, push this thread into run queue and
    // - make it ready for the execution.

    //init new node
    node *newThread = (node *) malloc(sizeof(node));
    newThread->data = newTCB;
    newThread->next = NULL;

    //enqueue new thread
    enqueue(run_Q, newThread);

    // if(init_scheduler_done ==0){
    //     scheduler();
    // }
    // getcontext(*currContext); 
    // *currContext = schedContext;

    
    if(init_scheduler_done == 0){//only need to swap to scheduler the first time
        init_scheduler_done =1;
        swapcontext(currentThread->data->context, schedTCB->context); //save main context, swap to sched
    }
    return 0;
}

/* give CPU possession to other user-level worker threads voluntarily */
//FINISH
int worker_yield()
{
    //stop timer
    disarmTimer();
    //move to tail of runQ. right now it's dequeued from runQ
    enqueue(run_Q, currentThread);
    //swap context to scheduler
        //problem: if i save context here, when it comes back, it will swap to the scheduler again?
        //nah we straight i believe
    swapcontext(currentThread->data->context, schedTCB->context);

    return 0;
    // - change worker thread's state from Running to Ready
    // - save context of this thread to its thread control block
    // - switch from thread context to scheduler context
}

/* terminate a thread */
void worker_exit(void *value_ptr)
{
    disarmTimer();
    // - if value_ptr is provided, save return value
    

    //make sure to check if any threads are waiting on the exitted thread
         //if not free this thread
         //else just return. they will eventually join.
       
    if(currentThread->data->headOfJoiningQ == NULL){
        //free this thread
        node* toFree = currentThread; //free toFree instead of currentThread so that currentThread is stilll usable after
        currentThread = NULL;
        toFree->data->retval = (int) value_ptr;
        free(toFree->data->context);
        free(toFree->data); //joiningQ is null so dont need to free it
        free(toFree);
    }
    return;
    //pop head of joiningQ

}

/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr)
{//spinlock
    // - wait for a specific thread to terminate

    //search for thread in runQ
    //check if status is TERMINATED
        //if so, save retval, remove it from Q, and free it
        //if not, add caller thread to joiningQ. enter spinlock that continually checks for TERMINATED 

    // - if value_ptr is provided, retrieve return value from joining thread
    // - de-allocate any dynamic memory created by the joining thread
    return 0;

};

/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex,const pthread_attr_t *mutexattr)
{
    //- initialize data structures for this mutex
    return 0;

};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex)
{

    // - use the built-in test-and-set atomic function to test the mutex
    // - if the mutex is acquired successfully, enter the critical section
    // - if acquiring mutex fails, push current thread into block list and
    // context switch to the scheduler thread
    return 0;

};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex)
{
    // - release mutex and make it available again.
    // - put one or more threads in block list to run queue
    // so that they could compete for mutex later.

    return 0;
};

/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex)
{
    // - make sure mutex is not being used
    // - de-allocate dynamic memory created in worker_mutex_init

    return 0;
};

/* scheduler */
static void schedule()
{

//times when scheduler is called
    //timer interrupt
    //thread termination
    //thread yeild

//dont need to set currTCB to scheduler.

//currentThread is NULL if a thread just exitted

//never swapcontext. just setcontext.


// - every time a timer interrupt occurs, your worker thread library
// should be contexted switched from a thread context to this
// schedule() function

    //determine which thread to run
    //update queues
    //if runQ is not empty ie it's not the first time running sechule()
    if(run_Q->head){
        //dequeue from runQ, enqueue to tail of ready Q
    }
    //pop from head of readyQ and push onto runQ


    //set up timer signal
    timer.it_interval.tv_usec = 0; 
	timer.it_interval.tv_sec = 1;
	timer.it_value.tv_usec = 0;
	timer.it_value.tv_sec = 1;
    //start timer
    setitimer(ITIMER_PROF, &timer, NULL);
    //switch context of next thread


// - invoke scheduling algorithms according to the policy (RR or MLFQ)

// - schedule policy
#ifndef MLFQ
    // Choose RR
    sched_rr();
    
#else
    // Choose MLFQ
    
#endif
}

static void sched_rr()
{
    // - your own implementation of RR
    // (feel free to modify arguments and return types)

}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq()
{
    // - your own implementation of MLFQ
    // (feel free to modify arguments and return types)

}

// Feel free to add any other functions you need.
// You can also create separate files for helper functions, structures, etc.
// But make sure that the Makefile is updated to account for the same.