#define _XOPEN_SOURCE 600


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <ucontext.h>

#define STACK_SIZE SIGSTKSZ

ucontext_t bench, first, second;
int check=1;

void scheduler(int signum){
	if(check){
		check--;
		swapcontext(&first,&second);
	}else{
		check++;
		swapcontext(&second,&first);
	}
}


void foo(){
	while(1){
		printf("foo");
		sleep(.5);
	}
}

void bar(){	
	while(1){
		printf("bar");
		sleep(.5);
	}
}

int main(){
	
	//set up foo and bar contexts
	getcontext(&first);
	getcontext(&second);

	void* stack1 = malloc(STACK_SIZE);
	void* stack2 = malloc(STACK_SIZE);

	first.uc_link=NULL;
	first.uc_stack.ss_sp=stack1;
	first.uc_stack.ss_size=STACK_SIZE;
	first.uc_stack.ss_flags=0;

	second.uc_link=NULL;
	second.uc_stack.ss_sp=stack2;
	second.uc_stack.ss_size=STACK_SIZE;
	second.uc_stack.ss_flags=0;

	makecontext(&first,(void *)&foo,0);
	makecontext(&second,(void *)&bar,0);

	//set up timer signals
	struct itimerval timer;

	timer.it_interval.tv_usec = 0; 
	timer.it_interval.tv_sec = 1;

	timer.it_value.tv_usec = 0;
	timer.it_value.tv_sec = 1;
	

	struct sigaction sa;
	memset (&sa, 0, sizeof (sa));
	sa.sa_handler = &scheduler;
	sigaction(SIGPROF, &sa, NULL);

	setitimer(ITIMER_PROF, &timer, NULL);
	swapcontext(&bench, &first);
}