/* sh6bench.c -- SmartHeap (tm) Portable memory management benchmark.
 *
 * Copyright (C) 2000 MicroQuill Software Publishing Corporation.
 * All Rights Reserved.
 *
 * No part of this source code may be copied, modified or reproduced
 * in any form without retaining the above copyright notice.
 * This source code, or source code derived from it, may not be redistributed
 * without express written permission of the copyright owner.
 *
 *
 * Compile-time flags.  Define the following flags on the compiler command-line
 * to include the selected APIs in the benchmark.  When testing an ANSI C
 * compiler, include MALLOC_ONLY flag to avoid any SmartHeap API calls.
 * Define these symbols with the macro definition syntax for your compiler,
 * e.g. -DMALLOC_ONLY=1 or -d MALLOC_ONLY=1
 *
 *  Flag                   Meaning
 *  -----------------------------------------------------------------------
 *  MALLOC_ONLY=1       Test ANSI malloc/realloc/free only
 *  INCLUDE_NEW=1       Test C++ new/delete
 *  INCLUDE_MOVEABLE=1  Test SmartHeap handle-based allocation API
 *  MIXED_ONLY=1        Test interdispersed alloc/realloc/free only
 *                      (no tests for alloc, realloc, free individually)
 *  SYS_MULTI_THREAD=1  Test with multiple threads (OS/2, NT, HP, Solaris only)
 *  SMARTHEAP=1         Required when compiling if linking with SmartHeap lib
 * 
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <limits.h>


#ifdef __cplusplus
extern "C"
{
#endif

/* Unix prototypes */
#ifndef UNIX
#define UNIX 1
#endif

#include <unistd.h>
#define _INCLUDE_POSIX_SOURCE
#include <sys/signal.h>
#include <pthread.h>
typedef pthread_t ThreadID;
#include <sys/sysinfo.h>
int thread_specific;

#ifndef THREAD_NULL
#define THREAD_NULL 0
#endif
#ifndef THREAD_EQ
#define THREAD_EQ(a,b) ((a)==(b))
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#include "AllocatorMacro.hpp"

#ifdef SILENT
void fprintf_silent(FILE *, ...);
void fprintf_silent(FILE *x, ...) { (void)x; }
#else
#define fprintf_silent fprintf
#endif

#ifndef min
#define min(a,b)    (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b)    (((a) > (b)) ? (a) : (b))
#endif

#ifdef CLK_TCK
#undef CLK_TCK
#endif
#define CLK_TCK CLOCKS_PER_SEC

#define TRUE 1
#define FALSE 0
typedef int Bool;

FILE *fout, *fin;
unsigned uMaxBlockSize = 1000;
unsigned uMinBlockSize = 1;
unsigned long ulCallCount = 1000;

unsigned long promptAndRead(char *msg, unsigned long defaultVal, char fmtCh);

unsigned uThreadCount = 8;
ThreadID RunThread(void (*fn)(void *), void *arg);
void WaitForThreads(ThreadID[], unsigned);
int GetNumProcessors(void);



inline uint64_t rdtsc(void) {
	unsigned int hi, lo;
	__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
	return ((uint64_t) lo) | (((uint64_t) hi) << 32);
}

const int64_t kCPUSpeed = 2000000000;

void doBench(void *);

pthread_barrier_t barrier;

int main(int argc, char *argv[])
{
	clock_t startCPU;
	time_t startTime;
	double elapsedTime, cpuTime;

	uint64_t start_;
	uint64_t end_;

	setbuf(stdout, NULL);  /* turn off buffering for output */

	if (argc > 1)
		fin = fopen(argv[1], "r");
	else
		fin = stdin;
	if (argc > 2)
		fout = fopen(argv[2], "w");
	else
		fout = stdout;

	ulCallCount = promptAndRead("call count", ulCallCount, 'u');
	uMinBlockSize = (unsigned)promptAndRead("min block size",uMinBlockSize,'u');
	uMaxBlockSize = (unsigned)promptAndRead("max block size",uMaxBlockSize,'u');


	unsigned i;
	int *threadArg = malloc(uThreadCount*sizeof(int));
	ThreadID *tids;

	uThreadCount = (int)promptAndRead("threads", GetNumProcessors(), 'u');
	pthread_barrier_init(&barrier,NULL,uThreadCount);
	pm_init();

	printf("\nparams: call count: %u, min size: %u, max size: %u, threads: %u\n", ulCallCount, uMinBlockSize, uMaxBlockSize, uThreadCount);

	if (uThreadCount < 1)
		uThreadCount = 1;
	ulCallCount /= uThreadCount;
	if ((tids = malloc(sizeof(ThreadID) * uThreadCount)) != NULL){
		startCPU = clock();
		startTime = time(NULL);
		start_ = rdtsc();
		for (i = 0;  i < uThreadCount;  i++){
			threadArg[i] = i;
			if (THREAD_EQ(tids[i] = 
				RunThread(doBench, &threadArg[i]),THREAD_NULL)){
				fprintf(fout, "\nfailed to start thread #%d", i);
				break;
			}
		}
		WaitForThreads(tids, uThreadCount);
		free(tids);
	}
	if (threadArg)
		free(threadArg);

	end_ = rdtsc();
	elapsedTime = difftime(time(NULL), startTime);
	cpuTime = (double)(clock()-startCPU) / (double)CLOCKS_PER_SEC;

	fprintf_silent(fout, "\n");
	fprintf(fout, "\nTotal elapsed time"
			  " for %d threads"
			  ": %.2f (%.4f CPU)\n",
			  uThreadCount,
			  elapsedTime, cpuTime);

	fprintf(fout, "\nrdtsc time: %f\n", ((double)end_ - (double)start_)/kCPUSpeed);

	if (fin != stdin)
		fclose(fin);
	if (fout != stdout)
		fclose(fout);
	pm_close();
	return 0;
}

void doBench(void *arg)
{ 
#ifdef THREAD_PINNING
    int task_id;
    int core_id;
    cpu_set_t cpuset;
    int set_result;
    int get_result;
    CPU_ZERO(&cpuset);
    task_id = *(int*)arg;
    core_id = PINNING_MAP[task_id%80];
    CPU_SET(core_id, &cpuset);
    set_result = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (set_result != 0){
    	fprintf(stderr, "setaffinity failed for thread %d to cpu %d\n", task_id, core_id);
	exit(1);
    }
    get_result = pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (set_result != 0){
    	fprintf(stderr, "getaffinity failed for thread %d to cpu %d\n", task_id, core_id);
	exit(1);
    }
    if (!CPU_ISSET(core_id, &cpuset)){
   	fprintf(stderr, "WARNING: thread aiming for cpu %d is pinned elsewhere.\n", core_id);	 
    } else {
    	// fprintf(stderr, "thread pinning on cpu %d succeeded.\n", core_id);
    }
	pthread_barrier_wait(&barrier);

#endif
	char **memory = pm_malloc(ulCallCount * sizeof(void *));
	int	size_base, size, iterations;
	int	repeat = ulCallCount;
	char **mp = memory;
	char **mpe = memory + ulCallCount;
	char **save_start = mpe;
	char **save_end = mpe;

	while (repeat--){ 
	for (size_base = uMinBlockSize;
		 size_base < uMaxBlockSize;
		 size_base = size_base * 3 / 2 + 1){
		for (size = size_base; size >= uMinBlockSize; size /= 2){
			/* allocate smaller blocks more often than large */
			iterations = 1;

			if (size < 10000)
				iterations = 10;

			if (size < 1000)
				iterations *= 5;

			if (size < 100)
				iterations *= 5;

			while (iterations--){ 
				if (!memory || !(*mp = (char *)pm_malloc(size))){
					printf("Out of memory\n");
					_exit (1);
				}
				mp++;
		/* while allocating skip over that portion of the buffer that still
		 * holds pointers from the previous cycle
		 */
				if (mp == save_start)
					mp = save_end;

				if (mp >= mpe){
					/* if we've reached the end of the malloc buffer */
					mp = memory;
					/* mark the next portion of the buffer */
					save_start = save_end;  
					if (save_start >= mpe) save_start = mp;
					save_end = save_start + (ulCallCount / 5);
					if (save_end > mpe) save_end = mpe;
			/* free the bottom and top parts of the buffer.
			 * The bottom part is freed in the order of allocation.
			 * The top part is free in reverse order of allocation.
			 */
					while (mp < save_start){
						pm_free (*mp);
						mp++;
					}
					mp = mpe;
					while (mp > save_end) {
						mp--;
						pm_free (*mp);
					}
					if(save_start == memory){
						mp = save_end;
					} else{
						mp = memory;
					}
				}
			}
		}
	}
	}
	/* free the residual allocations */
	mpe = mp;
	mp = memory;

	while (mp < mpe){
		pm_free (*mp);
		mp++;
	}

	pm_free(memory);
}

unsigned long promptAndRead(char *msg, unsigned long defaultVal, char fmtCh)
{
	char *arg = NULL, *err;
	unsigned long result;
	{
		char buf[12];
		static char fmt[] = "\n%s [%lu]: ";
		fmt[7] = fmtCh;
		fprintf_silent(fout, fmt, msg, defaultVal);
		if (fgets(buf, 11, fin))
			arg = &buf[0];
	}
	if (arg && ((result = strtoul(arg, &err, 10)) != 0
					|| (*err == '\n' && arg != err))){
		return result;
	}
	else
		return defaultVal;
}


/*** System-Specific Interfaces ***/

ThreadID RunThread(void (*fn)(void *), void *arg)
{
	ThreadID result = THREAD_NULL;
	
	pthread_attr_t attr;
	pthread_attr_init(&attr);
#ifdef RALLOC
	if (pthread_create(&result, &attr, (void *(*)(void *))fn, arg) == -1)
#elif defined (MAKALU)
	if (MAK_pthread_create(&result, &attr, (void *(*)(void *))fn, arg) == -1)
#else
	if (pthread_create(&result, &attr, (void *(*)(void *))fn, arg) == -1)
#endif
		return THREAD_NULL;
	return result;
}

/* wait for all benchmark threads to terminate */
void WaitForThreads(ThreadID tids[], unsigned tidCnt)
{
	while (tidCnt--)
		pthread_join(tids[tidCnt], NULL);
}

/* return the number of processors present */
int GetNumProcessors()
{
	return get_nprocs();
}
