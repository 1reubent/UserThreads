#include <stdio.h>
#include <unistd.h>
#include "../thread-worker.h"

#define DEFAULT_THREAD_NUM 3




void dummy_work(void *arg)
{
	int i = 0;
    int j = 0;
	int n = *((int *)arg);

    for(i=0;i<20;i++){
        for(j=0;j<10000000;j++){
        }
        printf("Thread %d running\n",n);
		worker_yield();
    }

    printf("Thread %d exiting\n",n);
	worker_exit(NULL);
}


int main(int argc, char **argv)
{
	int thread_num;
if (argc == 1)
	{
		thread_num = DEFAULT_THREAD_NUM;
	}
	else
	{
		if (argv[1] < 1)
		{
			printf("enter a valid thread number\n");
			return 0;
		}
		else
		{
			thread_num = atoi(argv[1]);
		}
	}

    printf("Running main thread\n");

    int i=0;

	int *counter = (int *)malloc(thread_num * sizeof(int));
	for (i = 0; i < thread_num; i++){
		counter[i] = i+1;
    }

	worker_t *thread = (worker_t *)malloc(thread_num * sizeof(worker_t));

	for (i = 0; i < thread_num; i++){
		printf("Main thread creating worker thread %d\n", counter[i]);
        worker_create(&thread[i], NULL, &dummy_work, &counter[i]);
    }

	for (i = 0; i < thread_num; i++){
        printf("Main thread waiting on thread %d\n",counter[i]);
		worker_join(thread[i], NULL);
    }

    printf("Main thread resume\n");
	free(thread);
	free(counter);
	
    printf("Main thread exit\n");
	return 0;
}
