// File:	thread-worker.c

// List all group member's name: omer sen (netID: os226) , reuben thomas (netID: rmt135)
/*
 */
// username of iLab: 
// iLab Server:cpp.cs.rugters.edu

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <ucontext.h>
#include "thread-worker.h"
#include "thread_worker_types.h"
#include "mutex_types.h"

#define STACK_SIZE 16 * 1024
#define QUANTUM 10 * 1000
#define DEBUG


// INITIALIZE ALL YOUR OTHER VARIABLES HERE
int init_scheduler_done = 0;
int mainThreadCreated = 0;
struct itimerval timer;
struct itimerval tempTimer; //for holding the timer state during pauseTimer()


int currentTID =1; //use as the tid of the next new thread. continually increment
tcb *schedTCB; //tcb of the scheduler. scheduler doesnt have a node. tid is always 0.
node* currentThread; //node of the currently running thread
//node* toFreeAfterExit; //if a thread calls exit AND has no parent, it needs to be freed by scheduler
int currentMID =0; //ids for current mutexes

//only need a runQ and maybe a waitQ, but maybe not.
queue *term_Q, *run_Q; //run Q and terminated Q

typedef struct mutNode {// Q of mutexes. functions like a stack, push and pop at the head.
	struct mutNode *next;
	worker_mutex_t* mut;
} mutNode;

mutNode *headOfMutQ;



void signal_handler(int signum){
    #ifdef DEBUG
        if(mainThreadCreated==-1){
            printf("main thread waited\n");
            mainThreadCreated=1;
        }
    #endif
        
    swapcontext(currentThread->data->context, schedTCB->context);
}

void freeThread(node** toFree){
    free((*toFree)->data->context->uc_stack.ss_sp); //main thread has no malloc'd stack tho?
    //free context
    free((*toFree)->data->context);
    //free tcb
    free((*toFree)->data);
    //free node
    free((*toFree));
}
//returns ptr
node* searchQ(queue *Q, worker_t toFind){
    //dont remove from gueu
    node* ptr = Q->head;

    while(ptr!=NULL && (unsigned) ptr->data->tid != (unsigned) toFind){
        ptr = ptr->next;
    }

    return ptr;
}
//enqueue function
int enqueue (queue *Q, node* threadNode) {
    if(searchQ(Q, threadNode->data->tid)!=NULL){
        perror("error");
        exit(1);
    }
    threadNode->next = NULL;
    //add to the tail of the queue
    if (Q->tail != NULL) {
        //enqueue behind tail
        Q->tail->next = threadNode;
        Q->tail = threadNode;
    }
    else{
        //empty Q
        Q->head = threadNode;
        Q->tail = threadNode;
    }
    (Q->size)++;

    return 0;
}

//FINISH
node* dequeue(queue *Q){
    //dequeue head
    node* removed =NULL;
    if (Q->size >= 1) {
        removed =Q->head;
        if(Q->size==1){
            Q->head = NULL;
            Q->tail = NULL;
        }else{
            Q->head= Q->head->next; 
        }
        (Q->size)--;
    }
    return removed;
}

node* removeNode(queue *Q, worker_t toRemove){
    node* ptr = Q->head;
    if(ptr == NULL){
        return NULL;
    }
    if( Q->head->data->tid == (unsigned) toRemove){ //if it's the head
        if(Q->size==1){
            Q->head = Q->tail = NULL;
        }else{
            Q->head= Q->head->next; 
        }
        (Q->size)--;
        return ptr;
    }
    
    node* prev = ptr;
    while(ptr!=NULL && (unsigned) (ptr->data->tid) != (unsigned) toRemove){ //traverse Q
        prev = ptr;
        ptr = ptr->next;
    }

    if(ptr!=NULL){
        if( Q->tail->data->tid == (unsigned) toRemove){ //if it's the tail
            prev->next =NULL;
            Q->tail = prev;
        }else{
            prev->next = ptr->next;
        }
        (Q->size)--;
    }
    
    return ptr;
}

int pushMut(mutNode* toPush){ //push mutNode into mutQ
    toPush->next = headOfMutQ;
    headOfMutQ = toPush;
    return 0;
}

mutNode* removeMut(worker_mutex_t *mutex){ //remove mutNode from mutQueue
    mutNode* ptr = headOfMutQ;
    if(ptr == NULL || mutex==NULL){
        return NULL;
    }

    if(headOfMutQ->mut->mid == mutex->mid){
        headOfMutQ = headOfMutQ->next;
        return ptr;
    }
    mutNode* prev = ptr;
    while(ptr!=NULL && ptr->mut->mid != mutex->mid){
        prev = ptr;
        ptr = ptr->next;
    }

    if(ptr!=NULL){
        prev->next = ptr->next;
    }

    return ptr;
}

node* searchMutQForThread(worker_t thread){ //search mutQ for specific thread in one of the mutexes waitQs
    mutNode* ptr = headOfMutQ;

    node* ptr2 = NULL;
    while(ptr!=NULL){ // for each mutex in mutQ, search for thread in its waitQ
        ptr2 = searchQ(ptr->mut->wait_Q, thread);
        if (ptr2 != NULL){
            break;
        }
        ptr = ptr->next;
    }

    return ptr2;


    
}

int searchMutQ(worker_mutex_t* mutex){
    if(mutex==NULL){
        return -1;
    }

    mutNode* ptr = headOfMutQ;
    while(ptr!=NULL && ptr->mut->mid != mutex->mid){ // search for mutex in mutQ
        ptr = ptr->next;
    }
    return (ptr==NULL) ? -1 : 0;
}

void restartTimer(){ //reset timer to QUANTUM
    timer.it_interval.tv_usec = QUANTUM; 
	timer.it_interval.tv_sec = 0;
	timer.it_value.tv_usec = QUANTUM;
	timer.it_value.tv_sec = 0;
    //arm timer
    setitimer(ITIMER_PROF, &timer, NULL);
}
void pauseTimer(){
    //save rmaining time in case it's needed later
    //getitimer(ITIMER_PROF, &tempTimer);

    timer.it_interval.tv_usec = 0; 
	timer.it_interval.tv_sec = 0;
	timer.it_value.tv_usec = 0;
	timer.it_value.tv_sec = 0;
    //disarm timer
    setitimer(ITIMER_PROF, &timer, &tempTimer);
}

void resumeTimer(){ //resume timer after pause

    if(tempTimer.it_value.tv_sec == 0 && tempTimer.it_value.tv_usec == 0 ){ //if the timer was somehow paused exactly at 0s0ms
        timer.it_interval.tv_usec = 0;  
        timer.it_interval.tv_sec = 0;
        timer.it_value.tv_usec = 1*1000; // 1ms
        timer.it_value.tv_sec = 0;
    }
    setitimer(ITIMER_PROF, &tempTimer, NULL);

}
static void sched_rr()
{
    //dequeue head of runQ
    currentThread = dequeue(run_Q);
    if(currentThread == NULL){
        perror("error");
        exit(1);
    }
    //set timer
    restartTimer();
    //switch to context of next thread
    setcontext(currentThread->data->context);
}

/* Preemptive MLFQ scheduling algorithm */
// static void sched_mlfq()
// {
//     // - your own implementation of MLFQ
//     // (feel free to modify arguments and return types)
//     return;

// }
/* scheduler */
//FIX: dont need to inc mainThreadCreated every time
//FIX: need to free main, queues etc.
static void schedule()
{
// - every time a timer interrupt occurs, your worker thread library
// should be contexted switched from a thread context to this
// schedule() function

//times when scheduler is called
    //first worker create, current thread is main. do not enqueue
    //timer interrupt, currentThread is normal. enqueue
    //thread termination (exit), currentThread is NULL. do not enqueue
    //thread yeild, currentThread is normal. do not enqueue

//never swapcontext. just setcontext.

    pauseTimer();
    
    //free oprhaned thread if there is one
    // if(toFreeAfterExit!=NULL){
    //     node* toFree = toFreeAfterExit;
    //     freeThread(&toFree);
    //     toFreeAfterExit = NULL;
    // }
    //enqueue current thread, as long as its not NULL or the
    if(currentThread!=NULL && mainThreadCreated!=0){
        enqueue(run_Q, currentThread);
    }else{
        mainThreadCreated++; //dont need to inc evey time FIX
    }
    /*CANT DO THIS FOLLOWING PART*/

    //check if main just joined the last thread. means we needa pack up, wrap it up tidy up
    // if(run_Q->size ==1 && currentThread==NULL){ //one thread left, and last thread has exited so currentThread==NULL
    //     //dont need to free structs used by scheduler or main? gets cleaned up after process exits
    //         //https://piazza.com/class/lrdvzfbsfvu32w/post/51
    //     //free main node and tcb.
    //     //free schedule tcb.
    //     //free queues
    //         //CHECK if there are any nodes in termQ that haven't been joined (orphans)
    //     if(term_Q->size !=0){
    //         //free all nodes
    //         node* toFree = dequeue(term_Q);
    //         while(toFree!=NULL){}
    //     }
    //     //free tiemr
    //     //free mutQ if it's not yet empty
    // }

    /*CANT DO IT bc there's know way for scheduler to know if main might call join in the future*/

    // mutex/waitQs management?
    //TERMQ stuff

    //if this is the last thread, need to free main thread, timer struct and queues

    // - invoke scheduling algorithms according to the policy (RR or MLFQ)

    // - schedule policy
    #ifndef MLFQ
        // Choose RR
        sched_rr();
        
    #else
        // Choose MLFQ
        
    #endif
}
// timer

void initSchedulerQsandTimer(){
    //init Qs
    term_Q = (queue*) malloc(sizeof(queue));
    if (term_Q == NULL){
		perror("Failed to allocate term_Q");
		exit(1);
	}
    term_Q->head = NULL;
    term_Q->tail = NULL;
    term_Q->size =0;

    run_Q = (queue*) malloc(sizeof(queue));
    if (run_Q == NULL){
		perror("Failed to allocate run_Q");
		exit(1);
	}
    run_Q->head = NULL;
    run_Q->tail =NULL;
    run_Q->size =0;

    
    // toFreeAfterExit = NULL;

    //init schdeuler context
    ucontext_t *schedContext = (ucontext_t*) malloc(sizeof(ucontext_t));
    if (schedContext == NULL){
		perror("Failed to allocate schedContext");
		exit(1);
	}
    if (getcontext(schedContext) < 0){
		perror("getcontext");
		exit(1);
	}
    void *stack=malloc(STACK_SIZE);
    if (stack == NULL){
		perror("Failed to allocate stack");
		exit(1);
	}
    schedContext->uc_link=NULL;
    schedContext->uc_stack.ss_sp=stack;
    schedContext->uc_stack.ss_size=STACK_SIZE;
    schedContext->uc_stack.ss_flags=0;

    makecontext(schedContext,&schedule,0);
    
    //init scheduler tcb
    schedTCB = (tcb *) malloc(sizeof(tcb));
    if (schedTCB == NULL){
		perror("Failed to allocate schedTCB");
		exit(1);
	}
    schedTCB->context = schedContext;
    schedTCB->status = SCHED;
    schedTCB->tid = 0;
    schedTCB->retval = 0;


    //init timer signal. schedule() is the signal handler
    struct sigaction sa;
	memset (&sa, 0, sizeof (sa));
	sa.sa_handler = &signal_handler;
	sigaction(SIGPROF, &sa, NULL);
}

//FIX: do not need a "parent" field in the tcb. every thread goes to termQ anyway and is freed by scheduler if not join/parent
/* create a new thread */
int worker_create(worker_t *thread, pthread_attr_t *attr, void *(*function)(void *), void *arg)
{
    //if first time calling woker_create
    if(init_scheduler_done ==0){
       
        //init scheduler thread, signal handler, + Qs
        initSchedulerQsandTimer();
        
        //initialize main thread
        //init context
        ucontext_t *mainContext = (ucontext_t*) malloc(sizeof(ucontext_t));
        if (mainContext == NULL){
		    perror("Failed to allocate mainContext");
		    exit(1);
	    }
        //mainContext->uc_stack.ss_sp = (void*) malloc(STACK_SIZE);
        // if (getcontext(mainContext) < 0){
        //     perror("getcontext");
		//     exit(1);
        // }
        // - allocate space of stack for this thread to run
        // mainContext->uc_stack.ss_size = STACK_SIZE;
        // mainContext->uc_link = NULL;
        // mainContext->uc_stack.ss_flags = 0;
            
        //init tcb
        tcb *mainTCB = (tcb *) malloc(sizeof(tcb));
        if (mainTCB == NULL){
		    perror("Failed to allocate mainTCB");
		    exit(1);
	    }
        mainTCB->context = mainContext;
        mainTCB->status = READY;
        mainTCB->tid = currentTID++;
        mainTCB->retval =0;

        //init queue node
        node *main = (node *) malloc(sizeof(node));
        if (main == NULL){
		    perror("Failed to allocate main");
		    exit(1);
	    }
        main->data = mainTCB;
        main->next = NULL;
       

        enqueue(run_Q, main);
        currentThread = main;

    }else{
        pauseTimer();
    }
    // - create and initialize the context of new worker thread
    
    ucontext_t *newThreadContext = (ucontext_t *) malloc(sizeof(ucontext_t));
    if (newThreadContext == NULL){
        perror("Failed to allocate newThreadContext");
        exit(1);
    }
    if (getcontext(newThreadContext) < 0){
		perror("getcontext");
		exit(1);
	}

    newThreadContext->uc_stack.ss_sp = (void *) malloc(STACK_SIZE);
    if (newThreadContext->uc_stack.ss_sp == NULL){
		perror("Failed to allocate stack");
		exit(1);
	}
    // - allocate space of stack for this thread to run
	newThreadContext->uc_stack.ss_size = STACK_SIZE;
	newThreadContext->uc_link = NULL;
    newThreadContext->uc_stack.ss_flags = 0;
		
    makecontext(newThreadContext, (void*) function, 1, arg);

    // - create new Thread Control Block (TCB)
    tcb *newTCB = (tcb *) malloc(sizeof(tcb));
    if (newTCB == NULL){
        perror("Failed to allocate newTCB");
        exit(1);
    }
    newTCB->context = newThreadContext;
    newTCB->status = READY;
    newTCB->tid = currentTID;
    newTCB->retval =0;
    //give tid to caller
    *thread = currentTID++;
    
    // after everything is set, push this thread into run queue and
    // - make it ready for the execution.

    //init new node
    node *newThread = (node *) malloc(sizeof(node));
    if (newThread == NULL){
        perror("Failed to allocate newThread");
        exit(1);
    }
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

        //enqueue main thread
        //enqueue(run_Q, currentThread); //enqueue it after the new thread, so it has a higher chance of joining it after

        swapcontext(currentThread->data->context, schedTCB->context); //save main context, swap to sched
        //setcontext(schedTCB->context);
    }else{
        resumeTimer();
    }
    return 0;
}

/* give CPU possession to other user-level worker threads voluntarily */
//FINISH
int worker_yield()
{
    //stop timer
    pauseTimer();
    //move to tail of runQ. right now it's dequeued from runQ
    // enqueue(run_Q, currentThread);
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
//FIX: FREETHREAD IS NOT WORKING
void worker_exit(void *value_ptr) //assuming every thread will eventually call this??
{
    pauseTimer();
    // - if value_ptr is provided, save return value

    //make sure to check if any threads are waiting on the exitted thread
         //if not free this thread
         //else just return. they will eventually join.
    
    /*
    THIS USED TO BE IN AN IF STATEMENT::
    */

    // if(currentThread->data->parent == NULL){
    //     //toFreeAfterExit = currentThread; //remove currentThread from runQ
    // }
    // else ::

    //put value_ptr in tcb for parent
    if(value_ptr != NULL){
        currentThread->data->retval = *( (int*)value_ptr );
    }
    //set status to TERMINATED
    currentThread->data->status = TERMINATED;
    enqueue(term_Q, currentThread);

    /*
    END OF USED-TO-BE IF STATEMENT
    */
    
    currentThread = NULL;
    setcontext(schedTCB->context); //swap to scheduler
}

/* Wait for thread termination */
//SHOULD CHECK TERMQ FOR THREAD
//FIX: freethread isnt working
//FIX: should probably swap to scheduler, and check if this is the last thread to join (just main left). need to free
int worker_join(worker_t thread, void **value_ptr)
{//spinlock
    // - wait for a specific thread to terminate

    //search for thread in runQ
    //check if status is TERMINATED
        //if so, save retval, remove it from Q, and free it
        //if not, add caller thread to thatthread->parent. enter spinlock that continually checks for TERMINATED 
    //IF thread is NOT found. then just return a 0 value_ptr. it means the thread is long gone.

    // - if value_ptr is provided, retrieve return value from joining thread
    // - de-allocate any dynamic memory created by the joining thread
    pauseTimer();

    node* child = removeNode(term_Q, thread); //returns NULL if not in Q
    
    if(child==NULL){
        child = searchQ(run_Q, thread);//if null, check runQ 
        if(child==NULL){ //if null, search mutex waitQs
            child = searchMutQForThread(thread);
        }
        if( child != NULL){ //should never be null but just in case
            resumeTimer();
            while(child->data->status != TERMINATED){
                #ifdef DEBUG
                    mainThreadCreated =-1;
                #endif
            } //spinlock until TERMINATED
            pauseTimer();

            child = removeNode(term_Q, thread); //remove child from termQ
        }else{
            perror("attempt to join with nonexistent thread");
            exit(1); //thread does not currently exist
        }
    }

    
    //save retval if value_ptr is not NULL
    if(value_ptr !=NULL){
         **( (int**) value_ptr) =child->data->retval;
    }
    
    //int** ret_ptr = (int**) value_ptr;
    //int* ret_val = malloc(sizeof(int));
    //*ret_val = child->data->retval;
    
    //free thread
    freeThread(&child);

    resumeTimer();
    return 0;

};


/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex,const pthread_attr_t *mutexattr)
{
    pauseTimer();
    //- initialize data structures for this mutex

    //memory for mutex already allocated
    mutex->mut = 0; 
    mutex->mid = currentMID++;
    mutex->currentUser =NULL;
    mutex->wait_Q = (queue*) malloc(sizeof(queue));
    if (mutex->wait_Q == NULL){
        perror("Failed to allocate mutex->wait_Q");
        exit(1);
    }
    mutex->wait_Q->head =NULL;
    mutex->wait_Q->tail =NULL;
    
    mutex->wait_Q->size =0;

    mutNode* newMutex = malloc(sizeof(mutNode));
    if (newMutex == NULL){
        perror("Failed to allocate newMutex");
        exit(1);
    }
    newMutex->mut = mutex;
    newMutex->next = NULL;
    pushMut(newMutex);

    resumeTimer();
    return 0;

};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex)
{
    // - use the built-in test-and-set atomic function to test the mutex
    // - if the mutex is acquired successfully, enter the critical section
    // - if acquiring mutex fails, push current thread into block list and
    // context switch to the scheduler thread
        //
    pauseTimer();
    //check if mutex exists AND that this thread doesn't already have the lock
    if(searchMutQ(mutex) != 0 && mutex->currentUser!= NULL && mutex->currentUser->data->tid != currentThread->data->tid){
        //resumeTimer();
        perror("invalid mutex lock");
        exit(1);
    }
    if(__sync_lock_test_and_set(&(mutex->mut), 1) ==0){
        //we got the lock
        mutex->currentUser = currentThread;
        resumeTimer();
        return 0;
    }
    //we dont got the lock
    //push currentThread to waitQ
    node* thisThread = currentThread;
    do{
        thisThread = currentThread;
        enqueue(mutex->wait_Q, currentThread);
        currentThread->data->status = WAITING;
        //new ptr to currentThread bc currentThread will be NULL
        
        currentThread = NULL;
        //swap and wait till it's your turn with the mutex
        swapcontext(thisThread->data->context, schedTCB->context);

        //thread comes back here
        pauseTimer();
    }while(__sync_lock_test_and_set(&(mutex->mut), 1) !=0); //if you come back and someone took the lock again, go back in waitQ
    
    //at this point, thread has the mutex

    mutex->currentUser = thisThread;
    currentThread->data->status = READY; //DOESNT HAVE TO BE HERE bc if thread is runing, its alrady in ready state by unlock
    resumeTimer();
    return 0;

};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex)
{
    // - release mutex and make it available again.
    // - put one or more threads in block list to run queue
    // so that they could compete for mutex later.
        
    pauseTimer();
    //check if mutex exists AND that this thread actually holds the lock
    if(searchMutQ(mutex) != 0 && mutex->currentUser!= NULL && mutex->currentUser->data->tid == currentThread->data->tid){
        //resumeTimer();
        perror("invalid mutex unlock");
        exit(1);
    }
    __sync_lock_release(&(mutex->mut)); //release mutex
    mutex->currentUser=NULL;
    //pop waitQ and put that thread into runQ
    node* nextUp = dequeue(mutex->wait_Q);
    if(nextUp!=NULL){
        //somebody wants mutex
        currentThread->data->status = READY;
        enqueue(run_Q, nextUp);
    }

    resumeTimer();
    return 0;
};

/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex)
{
    // - make sure mutex is not being used
    // - de-allocate dynamic memory created in worker_mutex_init
    pauseTimer();
    //check if mutex exists
    if(searchMutQ(mutex) != 0){
        //resumeTimer();
        perror("invalid mutex destroy");
        exit(1);
    }
    if(mutex->currentUser!=NULL ||mutex->wait_Q->size !=0 ){
        perror("attempt to destroy an occupied or requested mutex\n");
        exit(1);
    }

    free(mutex->wait_Q);
    mutNode* toFree =removeMut(mutex);
    if(toFree ==NULL){
        return -1;
    }
    free(toFree);

    resumeTimer();
    return 0;
};




// Feel free to add any other functions you need.
// You can also create separate files for helper functions, structures, etc.
// But make sure that the Makefile is updated to account for the same.