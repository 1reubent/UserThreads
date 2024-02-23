#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <ucontext.h>

#define STACK_SIZE SIGSTKSZ

ucontext_t main_ctx, t1_ctx, t2_ctx;

void *f1()
{
	puts("Thread 1 prints 1\n");
	puts("Thread 1 prints 2\n");
	swapcontext(&t1_ctx, &t2_ctx);
}

void *f2()
{
	puts("Thread 2 prints 3\n");
	puts("Thread 2 prints 4\n");
	puts("Thread 2 returning context to main\n");
	swapcontext(&t2_ctx, &main_ctx);
}

int main(int argc, char **argv)
{
	ucontext_t cctx, nctx;

	if (argc != 1)
	{
		printf(": USAGE Program Name and no Arguments expected\n");
		exit(1);
	}

	if (getcontext(&t1_ctx) < 0)
	{
		perror("getcontext");
		exit(1);
	}

	if (getcontext(&t2_ctx) < 0)
	{
		perror("getcontext");
		exit(1);
	}

	// Allocate space for stack
	void *stack1 = malloc(STACK_SIZE);

	if (stack1 == NULL)
	{
		perror("Failed to allocate stack");
		exit(1);
	}

	void *stack2 = malloc(STACK_SIZE);

	if (stack2 == NULL)
	{
		perror("Failed to allocate stack");
		exit(1);
	}

	/* Setup context that we are going to use */
	t1_ctx.uc_link = NULL;
	t1_ctx.uc_stack.ss_sp = stack1;
	t1_ctx.uc_stack.ss_size = STACK_SIZE;
	t1_ctx.uc_stack.ss_flags = 0;

	t2_ctx.uc_link = NULL;
	t2_ctx.uc_stack.ss_sp = stack2;
	t2_ctx.uc_stack.ss_size = STACK_SIZE;
	t2_ctx.uc_stack.ss_flags = 0;

	puts("allocate stack, attach func");

	// Make the context to start running at f1() and f2()
	makecontext(&t1_ctx, (void *)&f1, 0);
	makecontext(&t2_ctx, (void *)&f2, 0);

	puts("Successfully modified context\n");

	/* swap context will activate cctx and store location after swapcontext in nctx */
	puts("**************");
	puts("\nmain thread swapping to thread 1\n");
	swapcontext(&main_ctx, &t1_ctx);

	/* PC value in nctx will point to here */
	puts("swap context back to main executed correctly\n");
	puts("**************");

	return 0;
}
