How to run the benchmark?
-------------------------
First make sure that the thread library in the parent directory is already compiled. 
To do that first `cd` into `UserThreads/code`. Then `make` and `make clean`:
```
	$ cd UserThreads/code
	$ make clean
	$ make
```

1. Then, do a make clean and make
```
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
```


	Make sure to test your code with different user-level thread-worker thread counts. 
	We will test your code for large number (50-100) of user-level threads.

3. Test program

	The test program is also compiled when you run `make` as in step 1 mentioned above.
	This program can be run the same way as the other benchmarks:
```
	$ ./test
```
