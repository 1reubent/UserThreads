#include <stdio.h>
#include <unistd.h>
#include "../thread-worker.h"

#define DEFAULT_THREAD_NUM 3
#define SHORT_LOOP_COUNT 100
#define LONG_LOOP_COUNT 50

worker_mutex_t mutex;
worker_mutex_t mutex2;
int shared = 0;
int shortWorkers =0;
int longWorkers =0;

void dummy_work_short(void *arg)
{
	int i = 0;
	int j = 0;
	int n = *((int *)arg);

	for (i = 0; i < SHORT_LOOP_COUNT; i++)
	{
		for (j = 0; j < 30000000; j++)
		{
		}

		printf("Thread %d trying to lock mutex1\n", n);
		worker_mutex_lock(&mutex);
		printf("Thread %d acquired mutex1\n", n);

		printf("Thread %d trying to lock mutex2\n", n);
		worker_mutex_lock(&mutex2);
		printf("Thread %d acquired mutex2\n", n);

		int orig = shared;
		for (j = 0; j < 100; j++)
		{
		}
		shared = orig + 1;

		printf("Thread %d running\n", n);
		worker_yield();
		
		printf("Thread %d unlocking mutex2\n", n);
		worker_mutex_unlock(&mutex2);

		printf("Thread %d unlocking mutex\n", n);
		worker_mutex_unlock(&mutex);
	}

	int *ret = malloc(sizeof(int));
	*ret = 200 + n;
	printf("Thread %d exiting\n", n);
	worker_exit(ret);
}

void dummy_work_long(void *arg)
{
	int i = 0;
	int j = 0;
	int n = *((int *)arg);

	for (i = 0; i < LONG_LOOP_COUNT; i++)
	{
		for (j = 0; j < 30000000; j++)
		{
		}
		
		printf("Thread %d trying to lock mutex\n", n);
		worker_mutex_lock(&mutex);
		printf("Thread %d acquired mutex\n", n);

		printf("Thread %d trying to lock mutex2\n", n);
		worker_mutex_lock(&mutex2);
		printf("Thread %d acquired mutex2\n", n);

		int orig = shared;
		for (j = 0; j < 100; j++)
		{
		}
		shared = orig + 1;

		printf("Thread %d running long\n", n);

		printf("Thread %d unlocking mutex2\n", n);
		worker_mutex_unlock(&mutex2);

		printf("Thread %d unlocking mutex\n", n);
		worker_mutex_unlock(&mutex);
	}

	int *ret = malloc(sizeof(int));
	*ret = 700 + n;

	printf("Thread %d exiting\n", n);
	worker_exit(ret);
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

	int i = 0;

	int *counter = (int *)malloc(thread_num * sizeof(int));
	for (i = 0; i < thread_num; i++)
	{
		counter[i] = i + 1;
	}

	int **retvals = (int *)malloc(thread_num * sizeof(int *));
	
	/*ADDED THIS FORLOOP*/
	for (i = 0; i < thread_num; i++)
	{
		(retvals)[i] = (int *) malloc(sizeof(int *));
	}
	worker_t *thread = (worker_t *)malloc(thread_num * sizeof(worker_t));

	worker_mutex_init(&mutex, NULL);
	worker_mutex_init(&mutex2, NULL);

	for (i = 0; i < thread_num; i++)
	{
		printf("Main thread creating worker thread %d\n", counter[i]);
		if (i % 2 == 0)
		{
			shortWorkers++;
			worker_create(&thread[i], NULL, &dummy_work_short, &counter[i]);
		}
		else
		{
			longWorkers++;
			worker_create(&thread[i], NULL, &dummy_work_long, &counter[i]);
		}
	}

	for (i = 0; i < thread_num; i++)
	{
		printf("Main thread waiting on thread %d\n", counter[i]);
		worker_join(thread[i], &(retvals[i]));
	}

	printf("Main thread resume\n");

	for (i = 0; i < thread_num; i++)
	{
		if (retvals[i] != NULL)
		{
			printf("thread %d returned val %d\n", counter[i], *(retvals[i]));
		}
		else
		{
			printf("thread %d exited before main thread called join\n", counter[i]);
		}
	}

	worker_mutex_destroy(&mutex);
	worker_mutex_destroy(&mutex2);
	free(thread);
	free(counter);
	/*ADDED THIS FREE*/
	for (i = 0; i < thread_num; i++)
	{
		free((retvals)[i]);
	}
	free(retvals);

	printf("shared variable value: %d\n", shared);
	printf("expected value: %d\n",(SHORT_LOOP_COUNT*shortWorkers+LONG_LOOP_COUNT*longWorkers));
	printf("Main thread exit\n");
	return 0;
}
