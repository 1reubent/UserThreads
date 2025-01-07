How to run the benchmark?
-------------------------
First make sure that the thread library in the `code` directory is already compiled. 
To do that first `cd` into `UserThreads/code`. Then `make clean` and `make`:
```
	$ cd UserThreads/code
	$ make clean
	$ make
```
Then compile the benchamrks by `cd`ing into `UserThreads/code/benchmarks` and doing a `make clean` and `make`:
```
	$ cd UserThreads/code/benchmarks
	$ make clean
	$ make
```

2. Running benchmarks
```
	$ ./one_thread
```

```
	$ ./multiple_threads 6
```
Here 6 refers to the number of user-level threads to run. Similarly,
	
```
	$ ./multiple_threads_yield 6

	$ ./multiple_threads_mutex 6

	$ ./multiple_threads_different_workload 6

	$ ./multiple_threads_with_return 6

	$ ./test 6
```
