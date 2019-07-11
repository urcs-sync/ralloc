/*
 * This is a benchmark to test allocators in producer-consumer pattern.
 * Input $nthread$ must be even.
 * 
 * The benchmark is designed per the description of prod-con in the paper:
 *    Makalu: Fast Recoverable Allocation of Non-volatile Memory
 *    K. Bhandari et al.
 */

#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "fred.h"
#include "cpuinfo.h"
#include "timer.h"

#include "MichaelScottQueue.hpp"
#ifdef PMMALLOC

  #include "rpmalloc.hpp"
  #define pm_malloc(s) RP_malloc(s)
  #define pm_free(p) RP_free(p)

#elif defined(MAKALU) // PMMALLOC ends

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

// This class holds arguments to each thread.
class workerArg {
public:

  workerArg() {}

  workerArg (MichaelScottQueue<char*>* _msq, int _objNum, int _objSize)
    : msq (_msq),
      objNum (_objNum),
      objSize (_objSize)
  {}

  MichaelScottQueue<char*>* msq;
  int objNum;
  int objSize;
};

void * producer (void * arg)
{
	// Producer: allocate objNum of objects in size of ObjSize, push each to msq
	workerArg& w1 = *(workerArg *) arg;
	for (int i = 0; i < w1.objNum; i++) {
		// Allocate the object.
		char * obj = (char*)pm_malloc(sizeof(char)*w1.objSize);
		// Write into it
		for (int k = 0; k < w1.objSize; k++) {
			obj[k] = (char) k;
			volatile char ch = obj[k];
			ch++;
		}
		// push to msq
		w1.msq->enqueue(obj, 0);
	}
	return NULL;
}

void * consumer (void * arg)
{
	// Consumer: pop objects from msq, deallocate objNum of objects
	workerArg& w1 = *(workerArg *) arg;
	int i = 0;
	while(i < w1.objNum) {
		// pop from msq
		auto obj = w1.msq->dequeue(1);
		if(obj) {
			// deallocate it if not null
			pm_free(obj.value());
			i++;
		}
	}
	return NULL;
}

int main (int argc, char * argv[]){
	int nthreads;
	int objNum = 10000000;
	int objSize = 64; // byte

	if (argc > 3) {
		nthreads = atoi(argv[1]);
		objNum = atoi(argv[2]);
		objSize = atoi(argv[3]);
		if(nthreads%2!=0) {
			fprintf (stderr, "nthreads must be even\n");
			return 1;
		}
	} else {
		fprintf (stderr, "Usage: %s nthreads objNum objSize\n", argv[0]);
		return 1;
	}
	HL::Fred * threads = new HL::Fred[nthreads];
	HL::Fred::setConcurrency (HL::CPUInfo::getNumProcessors());
	std::vector<MichaelScottQueue<char*>*> msqs;
	std::vector<workerArg> wArg;
	int i;
	for (i = 0; i < nthreads/2; i++) {
		msqs.emplace_back(new MichaelScottQueue<char*>(2));
		wArg.emplace_back(msqs[i], objNum*2/nthreads, objSize);
		wArg.emplace_back(msqs[i], objNum*2/nthreads, objSize);
	}

#ifdef PMMALLOC
	RP_init("test");
#elif defined (MAKALU)
	__map_persistent_region();
	MAK_start(&__nvm_region_allocator);
#endif
	HL::Timer t;
	t.start();

	for(i = 0; i < nthreads/2; i++) {
		threads[i<<1].create (&producer, (void *) &wArg[i<<1]);
	}
	for(i = 0; i < nthreads/2; i++) {
		threads[(i<<1)+1].create (&consumer, (void *) &wArg[(i<<1)+1]);
	}
	for (i = 0; i < nthreads; i++) {
		threads[i].join();
	}

	t.stop();

	for(auto & m:msqs){
		delete m;
	}
	delete [] threads;
	printf ("Time elapsed = %f seconds.\n", (double) t);

#ifdef PMMALLOC
	RP_close();
#elif defined (MAKALU)
	MAK_close();
#endif
	return 0;
}