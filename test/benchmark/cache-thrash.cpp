///-*-C++-*-//////////////////////////////////////////////////////////////////
//
// Hoard: A Fast, Scalable, and Memory-Efficient Allocator
//        for Shared-Memory Multiprocessors
// Contact author: Emery Berger, http://www.cs.umass.edu/~emery
//
// Copyright (c) 1998-2003, The University of Texas at Austin.
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
 * @file  cache-thrash.cpp
 * @brief cache-thrash is a benchmark that exercises a heap's cache-locality.
 * This is about active false-sharing
 * Try the following (on a P-processor machine):
 *
 *  cache-thrash 1 1000 1 1000000
 *  cache-thrash P 1000 1 1000000
 *
 *  cache-thrash-hoard 1 1000 1 1000000
 *  cache-thrash-hoard P 1000 1 1000000
 *
 *  The ideal is a P-fold speedup.
*/


#include <iostream>
#include <stdlib.h>

using namespace std;

#include "cpuinfo.h"
#include "fred.h"
#include "timer.h"

#ifdef RALLOC

  #include "ralloc.hpp"
  #define pm_malloc(s) RP_malloc(s)
  #define pm_free(p) RP_free(p)

#elif defined(MAKALU) // RALLOC ends

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

#else // MAKALU ends

  #define pm_malloc(s) malloc(s)
  #define pm_free(p) free(p)

#endif //else ends



// This class just holds arguments to each thread.
class workerArg {
public:
  workerArg() {}
  workerArg (int objSize, int repetitions, int iterations)
    : _objSize (objSize),
      _iterations (iterations),
      _repetitions (repetitions)
  {}

  int _objSize;
  int _iterations;
  int _repetitions;
};


extern "C" void * worker (void * arg)
{
  // Repeatedly do the following:
  //   malloc a given-sized object,
  //   repeatedly write on it,
  //   then free it.
  workerArg * w = (workerArg *) arg;
  workerArg w1 = *w;
  for (int i = 0; i < w1._iterations; i++) {
    // Allocate the object.
    char * obj = (char*)pm_malloc(sizeof(char)*w1._objSize);
    //    printf ("obj = %p\n", obj);
    // Write into it a bunch of times.
    for (int j = 0; j < w1._repetitions; j++) {
      for (int k = 0; k < w1._objSize; k++) {
#if 0
	volatile double d = 1.0;
	d = d * d + d * d;
#else
	obj[k] = (char) k;
	volatile char ch = obj[k];
	ch++;
#endif
      }
    }
    // Free the object.
    pm_free(obj);
  }
  return NULL;
}


int main (int argc, char * argv[])
{
  int nthreads;
  int iterations;
  int objSize;
  int repetitions;

  if (argc > 4) {
    nthreads = atoi(argv[1]);
    iterations = atoi(argv[2]);
    objSize = atoi(argv[3]);
    repetitions = atoi(argv[4]);
  } else {
    cerr << "Usage: " << argv[0] << " nthreads iterations objSize repetitions" << endl;
    exit(1);
  }

  HL::Fred * threads = new HL::Fred[nthreads];
  HL::Fred::setConcurrency (HL::CPUInfo::getNumProcessors());

  int i;
#ifdef RALLOC
  RP_init("test");
#elif defined (MAKALU)
  __map_persistent_region();
  MAK_start(&__nvm_region_allocator);
#endif
  HL::Timer t;
  t.start();

  workerArg* wArg = new workerArg(objSize, repetitions / nthreads, iterations);
  for (i = 0; i < nthreads; i++) {
    threads[i].create (&worker, (void *) wArg);
  }
  for (i = 0; i < nthreads; i++) {
    threads[i].join();
  }
  t.stop();

  cout << "Time elapsed = " << (double) t << " seconds." << endl;
#ifdef RALLOC
  RP_close();
#elif defined (MAKALU)
  MAK_close();
#endif
}
