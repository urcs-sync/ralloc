///-*-C++-*-//////////////////////////////////////////////////////////////////
//
// Hoard: A Fast, Scalable, and Memory-Efficient Allocator
//        for Shared-Memory Multiprocessors
// Contact author: Emery Berger, http://www.cs.umass.edu/~emery
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
 * @file cache-scratch.cpp
 *
 * cache-scratch is a benchmark that exercises a heap's cache locality.
 * An allocator that allows multiple threads to re-use the same small
 * object (possibly all in one cache-line) will scale poorly, while
 * an allocator like Hoard will exhibit near-linear scaling.
 * This is about passive false sharing
 * Try the following (on a P-processor machine):
 *
 *  cache-scratch 1 1000 1 1000000
 *  cache-scratch P 1000 1 1000000
 *
 *  cache-scratch-hoard 1 1000 1 1000000
 *  cache-scratch-hoard P 1000 1 1000000
 *
 *  The ideal is a P-fold speedup.
*/

#include <stdio.h>
#include <stdlib.h>

#include "fred.h"
#include "cpuinfo.h"
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

  workerArg (char * obj, int objSize, int repetitions, int iterations)
    : _object (obj),
      _objSize (objSize),
      _iterations (iterations),
      _repetitions (repetitions)
  {}

  char * _object;
  int _objSize;
  int _iterations;
  int _repetitions;
};


extern "C" void * worker (void * arg)
{
  // free the object we were given.
  // Then, repeatedly do the following:
  //   malloc a given-sized object,
  //   repeatedly write on it,
  //   then free it.
  workerArg * w = (workerArg *) arg;
  pm_free(w->_object);
  workerArg w1 = *w;
  for (int i = 0; i < w1._iterations; i++) {
    // Allocate the object.
    char * obj = (char*)pm_malloc(sizeof(char)*w1._objSize);
    // Write into it a bunch of times.
    for (int j = 0; j < w1._repetitions; j++) {
      for (int k = 0; k < w1._objSize; k++) {
	obj[k] = (char) k;
	volatile char ch = obj[k];
	ch++;
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
    fprintf (stderr, "Usage: %s nthreads iterations objSize repetitions\n", argv[0]);
    return 1;
  }

  HL::Fred * threads = new HL::Fred[nthreads];
  HL::Fred::setConcurrency (HL::CPUInfo::getNumProcessors());

  int i;
#ifdef RALLOC
  RP_init("test");
  // tid = thread_count.fetch_add(1);
#elif defined (MAKALU)
  __map_persistent_region();
  MAK_start(&__nvm_region_allocator);
#endif
  // Allocate nthreads objects and distribute them among the threads.
  char ** objs = (char**)pm_malloc(sizeof(char *)*nthreads);
  for (i = 0; i < nthreads; i++) {
    objs[i] = (char*)pm_malloc(sizeof(char)*objSize);
  }

  HL::Timer t;
  t.start();

  workerArg* wArg;
  for (i = 0; i < nthreads; i++) {
    wArg = new workerArg(objs[i], objSize, repetitions / nthreads, iterations);
    threads[i].create (&worker, (void *) wArg);
  }
  for (i = 0; i < nthreads; i++) {
    threads[i].join();
  }
  t.stop();

  delete [] threads;
  pm_free(objs);

  printf ("Time elapsed = %f seconds.\n", (double) t);

#ifdef RALLOC
  RP_close();
#elif defined (MAKALU)
  MAK_close();
#endif
  return 0;
}
