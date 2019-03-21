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

#ifdef PMMALLOC

  #include "pmmalloc.hpp"
  #include "thread_util.hpp"
  pmmalloc* alloc = nullptr;

  #define pm_malloc(s) alloc->p_malloc(s)
  #define pm_free(p) alloc->p_free(p)

#elif defined (MAKALU)

  #include "makalu.h"
  #include <fcntl.h>
  #include <sys/mman.h>
  #define MAKALU_FILESIZE 5*1024*1024*1024ULL + 24
  #define pm_malloc(s) MAK_malloc(s)
  #define pm_free(p) MAK_free(p)
  #define HEAPFILE "/dev/shm/gc_heap_wcai6"

  char *base_addr = NULL;
  static char *curr_addr = NULL;

  void __map_persistent_region(){
      int fd; 
      fd  = open(HEAPFILE, O_RDWR | O_CREAT | O_TRUNC,
                    S_IRUSR | S_IWUSR);

      off_t offt = lseek(fd, MAKALU_FILESIZE-1, SEEK_SET);
      assert(offt != -1);

      int result = write(fd, "", 1); 
      assert(result != -1);

      void * addr =
          mmap(0, MAKALU_FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); 
      assert(addr != MAP_FAILED);

      *((intptr_t*)addr) = (intptr_t) addr;
      base_addr = (char*) addr;
      //adress to remap to, the root pointer to gc metadata, 
      //and the curr pointer at the end of the day
      curr_addr = (char*) ((size_t)addr + 3 * sizeof(intptr_t));
      printf("Addr: %p\n", addr);
      printf("Base_addr: %p\n", base_addr);
      printf("Current_addr: %p\n", curr_addr);
}
  int __nvm_region_allocator(void** memptr, size_t alignment, size_t size)
  {   
      char* next;
      char* res; 
      if (size < 0) return 1;
      
      if (((alignment & (~alignment + 1)) != alignment)  ||   //should be multiple of 2
          (alignment < sizeof(void*))) return 1; //should be atleast the size of void*
      size_t aln_adj = (size_t) curr_addr & (alignment - 1);
      
      if (aln_adj != 0)
          curr_addr += (alignment - aln_adj);
      
      res = curr_addr; 
      next = curr_addr + size;
      if (next > base_addr + MAKALU_FILESIZE){
          printf("\n----Ran out of space in mmaped file-----\n");
          return 1;
      }
      curr_addr = next;
      *memptr = res;
      //printf("Current NVM Region Addr: %p\n", curr_addr);
      
      return 0;
  }

#else

  #define pm_malloc(s) malloc(s)
  #define pm_free(p) free(p)

#endif

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
	void *threadArg = NULL;
	ThreadID *tids;

	uThreadCount = (int)promptAndRead("threads", GetNumProcessors(), 'u');
#ifdef PMMALLOC
	alloc = new pmmalloc("test",uThreadCount);
#elif defined (MAKALU)
	__map_persistent_region();
	MAK_start(&__nvm_region_allocator);
#endif

	printf("\nparams: call count: %u, min size: %u, max size: %u, threads: %u\n", ulCallCount, uMinBlockSize, uMaxBlockSize, uThreadCount);

	if (uThreadCount < 1)
		uThreadCount = 1;
	ulCallCount /= uThreadCount;
	if ((tids = malloc(sizeof(ThreadID) * uThreadCount)) != NULL){
		startCPU = clock();
		startTime = time(NULL);
		start_ = rdtsc();
		for (i = 0;  i < uThreadCount;  i++)
			if (THREAD_EQ(tids[i] = 
				RunThread(doBench, threadArg),THREAD_NULL)){
				fprintf(fout, "\nfailed to start thread #%d", i);
				break;
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
#ifdef PMMALLOC
	delete alloc;
#elif defined (MAKALU)
	MAK_close();
#endif
	return 0;
}

void doBench(void *arg)
{ 
	char **memory = pm_malloc(ulCallCount * sizeof(void *));
	int	size_base, size, iterations;
	int	repeat = ulCallCount;
	char **mp = memory;
	char **mpe = memory + ulCallCount;
	char **save_start = mpe;
	char **save_end = mpe;

	while (repeat--){ 
	for (size_base = 1;
		 size_base < uMaxBlockSize;
		 size_base = size_base * 3 / 2 + 1){
		for (size = size_base; size; size /= 2){
			/* allocate smaller blocks more often than large */
			iterations = 1;

			if (size < 10000)
				iterations = 10;

			if (size < 1000)
				iterations *= 5;

			if (size < 100)
				iterations *= 5;

			while (iterations--){ 
				if (!memory || !(*mp ++ = (char *)pm_malloc(size))){
					printf("Out of memory\n");
					_exit (1);
				}

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
					while (mp < save_start)
						pm_free (*mp ++);
					mp = mpe;
					while (mp > save_end) pm_free (*--mp);
					mp = memory;
				}
			}
		}
	}
	}
	/* free the residual allocations */
	mpe = mp;
	mp = memory;

	while (mp < mpe)
		pm_free (*mp ++);

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
#ifdef PMMALLOC
	if (pm_thread_create(&result, &attr, (void *(*)(void *))fn, arg) == -1)
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
