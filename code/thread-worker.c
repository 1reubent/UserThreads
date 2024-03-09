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
//#define DEBUG


// INITIALIZE ALL YOUR OTHER VARIABLES HERE
int init_scheduler_done = 0;
int mainThreadWaiting = 0;
struct itimerval timer;
struct itimerval tempTimer; //for holding the timer state during pauseTimer()


int currentTID =1; //use as the tid of the next new thread. continually increment
tcb *schedTCB; //tcb of the scheduler. scheduler doesnt have a node. tid is always 0.
node* currentThread; //node of the currently running thread
int currentMID =0; //ids for current mutexes

queue *term_Q, *run_Q; //run Q and terminated Q

typedef struct mutNode {// Q of mutexes. functions like a stack, push and pop at the head.
	struct mutNode *next;
	worker_mutex_t* mut;
} mutNode;

mutNode *headOfMutQ;



void signal_handler(int signum){
    #ifdef DEBUG
        if(mainThreadWaiting==-1){
            printf("MAIN THREAD WAITED.MAIN THREAD WAITED.MAIN THREAD WAITED\n");
            mainThreadWaiting=1;
        }
    #endif
        
    swapcontext(currentThread->data->context, schedTCB->context);
}

void freeThread(node** toFree){
    free((*toFree)->data->context->uc_stack.ss_sp);
    //free context
    free((*toFree)->data->context);
    //free tcb
    free((*toFree)->data);
    //free node
    free((*toFree));
}
//returns ptr
node* searchQ(queue *Q, worker_t toFind){
    if(Q ==NULL){
        perror("error. Q is NULL in searchQ() \n");
        exit(1);
    }
    node* ptr = Q->head;

    while(ptr!=NULL && (unsigned) ptr->data->tid != (unsigned) toFind){
        ptr = ptr->next;
    }

    return ptr;
}
//enqueue function
int enqueue (queue *Q, node* threadNode) {
    if(searchQ(Q, threadNode->data->tid)!=NULL){
        perror("error. enqueue-ing thread twice in a queue. \n");
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

node* dequeue(queue *Q){
    //dequeue head
    if(Q ==NULL){
        perror("error. Q is NULL in dequeue() \n");
        exit(1);
    }
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
    if(Q ==NULL){
        perror("error. Q is NULL in removeNode() \n");
        exit(1);
    }
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
    //save remaining time in case it's needed later
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
        perror("error\n");
        exit(1);
    }
    //set timer
    restartTimer();
    //switch to context of next thread
    setcontext(currentThread->data->context);
}

/* scheduler */

static void schedule()
{


    pauseTimer();
    
    //enqueue current thread, as long as its not NULL or the first time calling scheduler bc main is alreaady enqueued then
    if(currentThread!=NULL && init_scheduler_done!=0){
        enqueue(run_Q, currentThread);
    }else{
        init_scheduler_done=1; 
    }
        // - invoke scheduling algorithms according to the policy (RR or MLFQ)

    // - schedule policy
    #ifndef MLFQ
        // Choose RR
        sched_rr();
        
    #else
        // Choose MLFQ
        
    #endif
}

void initSchedulerQsandTimer(){
    //init Qs
    term_Q = (queue*) malloc(sizeof(queue));
    if (term_Q == NULL){
		perror("Failed to allocate term_Q\n");
		exit(1);
	}
    term_Q->head = NULL;
    term_Q->tail = NULL;
    term_Q->size =0;

    run_Q = (queue*) malloc(sizeof(queue));
    if (run_Q == NULL){
		perror("Failed to allocate run_Q\n");
		exit(1);
	}
    run_Q->head = NULL;
    run_Q->tail =NULL;
    run_Q->size =0;

    //init schdeuler context
    ucontext_t *schedContext = (ucontext_t*) malloc(sizeof(ucontext_t));
    if (schedContext == NULL){
		perror("Failed to allocate schedContext\n");
		exit(1);
	}
    if (getcontext(schedContext) < 0){
		perror("getcontext");
		exit(1);
	}
    void *stack=malloc(STACK_SIZE);
    if (stack == NULL){
		perror("Failed to allocate stack\n");
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
		perror("Failed to allocate schedTCB\n");
		exit(1);
	}
    schedTCB->context = schedContext;
    schedTCB->status = SCHED;
    schedTCB->tid = 0;
    schedTCB->retval = 0;


    //init timer signal.
    struct sigaction sa;
	memset (&sa, 0, sizeof (sa));
	sa.sa_handler = &signal_handler;
	sigaction(SIGPROF, &sa, NULL);
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
        ucontext_t *mainContext = (ucontext_t*) malloc(sizeof(ucontext_t));
        if (mainContext == NULL){
		    perror("Failed to allocate mainContext\n");
		    exit(1);
	    }
        //init tcb
        tcb *mainTCB = (tcb *) malloc(sizeof(tcb));
        if (mainTCB == NULL){
		    perror("Failed to allocate mainTCB\n");
		    exit(1);
	    }
        mainTCB->context = mainContext;
        mainTCB->status = READY;
        mainTCB->tid = currentTID++;
        mainTCB->retval =0;

        //init queue node
        node *main = (node *) malloc(sizeof(node));
        if (main == NULL){
		    perror("Failed to allocate main\n");
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
        perror("Failed to allocate newThreadContext\n");
        exit(1);
    }
    if (getcontext(newThreadContext) < 0){
		perror("getcontext\n");
		exit(1);
	}

    newThreadContext->uc_stack.ss_sp = (void *) malloc(STACK_SIZE);
    if (newThreadContext->uc_stack.ss_sp == NULL){
		perror("Failed to allocate stack\n");
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
        perror("Failed to allocate newTCB\n");
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
        perror("Failed to allocate newThread\n");
        exit(1);
    }
    newThread->data = newTCB;
    newThread->next = NULL;

    //enqueue new thread
    enqueue(run_Q, newThread);

    
    if(init_scheduler_done == 0){//only need to swap to scheduler the first time

        swapcontext(currentThread->data->context, schedTCB->context); //save main context, swap to sched
    }else{
        resumeTimer();
    }
    return 0;
}

/* give CPU possession to other user-level worker threads voluntarily */
int worker_yield()
{
    //stop timer
    pauseTimer();
    swapcontext(currentThread->data->context, schedTCB->context);

    return 0;
}


/* terminate a thread */
void worker_exit(void *value_ptr) //assuming every thread will eventually call this??
{
    pauseTimer();
    
    //put value_ptr in tcb for parent
    if(value_ptr != NULL){
        currentThread->data->retval = *( (int*)value_ptr );
    }
    //set status to TERMINATED
    currentThread->data->status = TERMINATED;
    enqueue(term_Q, currentThread);
    
    currentThread = NULL;
    setcontext(schedTCB->context); //swap to scheduler
}

/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr)
{
    pauseTimer();

    node* child = removeNode(term_Q, thread); //returns NULL if not in Q
    
    if(child==NULL){
        child = searchQ(run_Q, thread);//if null, check runQ 
        if(child==NULL){ //if null, search mutex waitQs
            child = searchMutQForThread(thread);
        }
        if( child != NULL){ 
            resumeTimer();
            while(child->data->status != TERMINATED){
                #ifdef DEBUG
                    mainThreadWaiting =-1;
                #endif
            } //spinlock until TERMINATED
            pauseTimer();

            child = removeNode(term_Q, thread); //remove child from termQ
        }else{
            perror("attempt to join with nonexistent thread\n");
            exit(1); //thread does not currently exist
        }
    }

    
    //save retval if value_ptr is not NULL
    if(value_ptr !=NULL){
         **( (int**) value_ptr) =child->data->retval;
    }
    
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
    if(mutex == NULL){
        perror("mutex is NULL\n");
        exit(1);
    }
    //memory for mutex already allocated
    mutex->mut = 0; 
    mutex->mid = currentMID++;
    mutex->currentUser =NULL;
    mutex->wait_Q = (queue*) malloc(sizeof(queue));
    if (mutex->wait_Q == NULL){
        perror("Failed to allocate mutex->wait_Q\n");
        exit(1);
    }
    mutex->wait_Q->head =NULL;
    mutex->wait_Q->tail =NULL;
    
    mutex->wait_Q->size =0;

    mutNode* newMutex = malloc(sizeof(mutNode));
    if (newMutex == NULL){
        perror("Failed to allocate newMutex\n");
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
    pauseTimer();
    //check if mutex exists AND that this thread doesn't already have the lock
    if(searchMutQ(mutex) != 0 || (mutex->currentUser!= NULL && mutex->currentUser->data->tid == currentThread->data->tid)){
        //resumeTimer();
        perror("invalid mutex lock\n");
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
    
    //at this point, thread has the mutex AND is in READY state

    mutex->currentUser = thisThread;
    resumeTimer();
    return 0;

};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex)
{
        
    pauseTimer();
    //check if mutex exists AND that this thread actually holds the lock
    if(searchMutQ(mutex) != 0 || mutex->currentUser== NULL || mutex->currentUser->data->tid != currentThread->data->tid){
        //resumeTimer();
        perror("invalid mutex unlock\n");
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
    pauseTimer();
    //check if mutex exists
    if(searchMutQ(mutex) != 0){
        //resumeTimer();
        perror("invalid mutex destroy\n");
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
