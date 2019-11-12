///-*-C++-*-//////////////////////////////////////////////////////////////////
//
// Hoard: A Fast, Scalable, and Memory-Efficient Allocator
//        for Shared-Memory Multiprocessors
// Contact author: Emery Berger, http://www.cs.utexas.edu/users/emery
//
// Copyright (c) 1998-2000, The University of Texas at Austin.
//
// This library is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as
// published by the Free Software Foundation, http://www.fsf.org.
//
// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
//////////////////////////////////////////////////////////////////////////////


/**
 * @file threadtest.cpp
 *
 * This program does nothing but generate a number of kernel threads
 * that allocate and free memory, with a variable
 * amount of "work" (i.e. cycle wasting) in between.
*/

#ifndef _REENTRANT
#define _REENTRANT
#endif

#include <iostream>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>


#include "fred.h"
#include "timer.h"
#include "AllocatorMacro.hpp"
int niterations = 50;	// Default number of iterations.
int nobjects = 30000;  // Default number of objects.
int nthreads = 1;	// Default number of threads.
int work = 0;		// Default number of loop iterations.
int sz = 1;


class Foo {
public:
  Foo (void)
    : x (14),
      y (29)
    {}

  int x;
  int y;
  void*
  operator new(std::size_t size) {
    return pm_malloc(size);
  }

  void*
  operator new[](std::size_t size) {
    return pm_malloc(size);
  }

  void
  operator delete(void *ptr) noexcept {
    pm_free(ptr);
  }

  void
  operator delete[](void *ptr) noexcept {
    pm_free(ptr);
  }
};



extern "C" void * worker (void * arg)
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
#endif
  int i, j;
  Foo ** a;
  a = new Foo * [nobjects / nthreads];
  for (j = 0; j < niterations; j++) {

    // printf ("%d\n", j);
    for (i = 0; i < (nobjects / nthreads); i ++) {
      a[i] = new Foo[sz];
      for (volatile int d = 0; d < work; d++) {
	volatile int f = 1;
	f = f + f;
	f = f * f;
	f = f + f;
	f = f * f;
      }
      assert (a[i]);
    }

    for (i = 0; i < (nobjects / nthreads); i ++) {
      delete[] a[i];
      for (volatile int d = 0; d < work; d++) {
	volatile int f = 1;
	f = f + f;
	f = f * f;
	f = f + f;
	f = f * f;
      }
    }
  }

  delete [] a;

  return NULL;
}

#if defined(__sgi)
#include <ulocks.h>
#endif

int main (int argc, char * argv[])
{
  HL::Fred * threads;
  //pthread_t * threads;

  if (argc >= 2) {
    nthreads = atoi(argv[1]);
  }

  if (argc >= 3) {
    niterations = atoi(argv[2]);
  }

  if (argc >= 4) {
    nobjects = atoi(argv[3]);
  }

  if (argc >= 5) {
    work = atoi(argv[4]);
  }

  if (argc >= 6) {
    sz = atoi(argv[5]);
  }
  pm_init();

  printf ("Running threadtest for %d threads, %d iterations, %d objects, %d work and %d sz...\n", nthreads, niterations, nobjects, work, sz);

  threads = new HL::Fred[nthreads];
  // threads = new hoardThreadType[nthreads];
  //  hoardSetConcurrency (nthreads);

  HL::Timer t;
  //Timer t;

  t.start ();

  int i;
  int *threadArg = malloc(nthreads*sizeof(int));
  for (i = 0; i < nthreads; i++) {
    threadArg[i] = i;
    threads[i].create (worker, &threadArg[i]);
  }

  for (i = 0; i < nthreads; i++) {
    threads[i].join();
  }
  t.stop ();

  printf( "Time elapsed = %f\n", (double) t);

  delete [] threads;
  pm_close();
  return 0;
}
