/*
 * Copyright (C) 2019 University of Rochester. All rights reserved.
 * Licenced under the MIT licence. See LICENSE file in the project root for
 * details. 
 */

#ifndef ALLOCATOR_MACRO
#define ALLOCATOR_MACRO
#include "pfence_util.h"


#ifndef THREAD_PINNING
#define THREAD_PINNING
#endif

#ifdef THREAD_PINNING
// current pinning map.
#define PINNING_MAP pinning_map_2x20a_1
// thread pinning strategy for 2x20a:
// 1 thread per core on one socket -> hyperthreads on the same socket -> cross socket.
static int pinning_map_2x20a_1[] = {
 	0,2,4,6,8,10,12,14,16,18,
 	20,22,24,26,28,30,32,34,36,38,
 	40,42,44,46,48,50,52,54,56,58,
 	60,62,64,66,68,70,72,74,76,78,
 	1,3,5,7,9,11,13,15,17,19,
 	21,23,25,27,29,31,33,35,37,39,
 	41,43,45,47,49,51,53,55,57,59,
 	61,63,65,67,69,71,73,75,77,79};

// thread pinning strategy for 2x20a:
// 5 cores on one socket -> 5 cores on the other ----> hyperthreads
static int pinning_map_2x20a_2[] = {
	0,2,4,6,8,1,3,5,7,9,
	10,12,14,16,18,11,13,15,17,19,
	20,22,24,26,28,21,23,25,27,29,
	30,32,34,36,38,31,33,35,37,39,
	40,42,44,46,48,41,43,45,47,49,
	50,52,54,56,58,51,53,55,57,59,
	60,62,64,66,68,61,63,65,67,69,
	70,72,74,76,78,71,73,75,77,79};

#endif
volatile static int init_count = 0;


#ifdef RALLOC

  #include "ralloc.hpp"
  inline void* pm_malloc(size_t s) { return RP_malloc(s); }
  inline void pm_free(void* p) { RP_free(p); }
  inline int pm_init() { return RP_init("test", 5*1024*1024*1024ULL + 24); }
  inline void pm_close() { RP_close(); }

#elif defined(MAKALU) // RALLOC ends

  #include "makalu.h"
  #include <fcntl.h>
  #include <sys/mman.h>
  #define MAKALU_FILESIZE (5*1024*1024*1024ULL + 24)
  inline void* pm_malloc(size_t s) { return MAK_malloc(s); }
  inline void pm_free(void* p) { MAK_free(p);}
  #ifdef SHM_SIMULATING
    #define HEAPFILE "/dev/shm/gc_heap_wcai6"
  #else
    #define HEAPFILE "/mnt/pmem/gc_heap_wcai6"
  #endif

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
  inline int pm_init() {
    __map_persistent_region();
    MAK_start(&__nvm_region_allocator);
    return 0;
  }

  inline void pm_close() { MAK_close(); }

#elif defined(PMDK)

  // No longer support PMDK since it's too slow
  #include <libpmemobj.h>
  #ifdef SHM_SIMULATING
    #define HEAPFILE "/dev/shm/pmdk_heap_wcai6"
  #else
    #define HEAPFILE "/mnt/pmem/pmdk_heap_wcai6"
  #endif
  #define PMDK_FILESIZE (5*1024*1024*1024ULL + 24)
  thread_local PMEMoid temp_ptr;
  PMEMobjpool* pop = nullptr;
  inline void* pm_malloc(size_t s) {
    int ret=pmemobj_alloc(pop, &temp_ptr, s, 0, nullptr,nullptr);
    if(ret==-1)return nullptr;
    return pmemobj_direct(temp_ptr);
  }
  inline void pm_free(void* p) {
    if(p==nullptr) return;
    temp_ptr = pmemobj_oid(p);
    pmemobj_free(&temp_ptr);
  }

  inline int pm_init() {
    pop = pmemobj_create(HEAPFILE, "test", PMDK_FILESIZE, 0666);
    if (pop == nullptr) {
      perror("pmemobj_create");
      return 1;
    }
    else return 0;
  }
  inline void pm_close() {
      pmemobj_close(pop);
  }

#else // MAKALU ends

  inline void* pm_malloc(size_t s) { return malloc(s); }
  inline void pm_free(void* p) { free(p);}
  inline int pm_init() { return 0; }
  inline void pm_close() { return; }

#endif //else ends

#endif
