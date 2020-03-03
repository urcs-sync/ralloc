/*
 * Copyright (C) 2019 University of Rochester. All rights reserved.
 * Licenced under the MIT licence. See LICENSE file in the project root for
 * details. 
 */

#ifndef ALLOCATOR_MACRO
#define ALLOCATOR_MACRO
#include "pfence_util.h"

#include <assert.h>

#ifndef THREAD_PINNING
#define THREAD_PINNING
#endif

#ifdef THREAD_PINNING
// current pinning map.
#define PINNING_MAP pinning_map_2x20a_1
// thread pinning strategy for 2x20a:
// 1 thread per core on one socket -> hyperthreads on the same socket -> cross socket.
static const int pinning_map_2x20a_1[] = {
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
static const int pinning_map_2x20a_2[] = {
	0,2,4,6,8,1,3,5,7,9,
	10,12,14,16,18,11,13,15,17,19,
	20,22,24,26,28,21,23,25,27,29,
	30,32,34,36,38,31,33,35,37,39,
	40,42,44,46,48,41,43,45,47,49,
	50,52,54,56,58,51,53,55,57,59,
	60,62,64,66,68,61,63,65,67,69,
	70,72,74,76,78,71,73,75,77,79};

// thread pinning strategy for 2x10c:
// 1 thread per core on one socket -> hyperthreads on the same socket -> cross socket.
static const int pinning_map_2x10c[] = {
 	0,2,4,6,8,10,12,14,16,18,
 	20,22,24,26,28,30,32,34,36,38,
 	1,3,5,7,9,11,13,15,17,19,
 	21,23,25,27,29,31,33,35,37,39};

#endif
volatile static int init_count = 0;

#define REGION_SIZE (6*1024*1024*1024ULL + 24)


#ifdef RALLOC

  #include "ralloc.hpp"
  inline void* pm_malloc(size_t s) { return RP_malloc(s); }
  inline void pm_free(void* p) { RP_free(p); }
  inline void* pm_realloc(void* ptr, size_t new_size) { return RP_realloc(ptr, new_size); }
  inline void* pm_calloc(size_t num, size_t size) { return RP_calloc(num, size); }
  inline int pm_init() { return RP_init("test", REGION_SIZE); }
  inline void pm_close() { RP_close(); }
  inline void pm_recover() { RP_recover(); }
  template<class T>
  inline T* pm_get_root(unsigned int i){
    return RP_get_root<T>(i);
  }
  inline void pm_set_root(void* ptr, unsigned int i) { RP_set_root(ptr, i); }

#elif defined(MAKALU) // RALLOC ends

  #include "makalu.h"
  #include <fcntl.h>
  #include <sys/mman.h>
  #include <sys/types.h>
  #include <unistd.h>
  #include <string.h>
  inline void* pm_malloc(size_t s) { return MAK_malloc(s); }
  inline void pm_free(void* p) { MAK_free(p);}
  #ifdef SHM_SIMULATING
    #define HEAP_FILE "/dev/shm/gc_heap_wcai6"
  #else
    #define HEAP_FILE "/mnt/pmem/gc_heap_wcai6"
  #endif

  static char *base_addr = NULL;
  static char *curr_addr = NULL;

  inline void* pm_realloc(void* ptr, size_t new_size) { 
    if(ptr == nullptr) return MAK_malloc(new_size);
    if(ptr<base_addr || ptr>curr_addr) return nullptr;
    void* new_ptr = MAK_malloc(new_size);
    if(new_ptr == nullptr) return nullptr;
    memcpy(new_ptr, ptr, MAK_get_size(ptr));
    FLUSH(new_ptr);
    FLUSHFENCE;
    MAK_free(ptr);
    return new_ptr;
  }

  inline void* pm_calloc(size_t num, size_t size) { 
    void* ptr = MAK_malloc(num*size);
    if(ptr == nullptr) return nullptr;
    memset(ptr, 0, MAK_get_size(ptr));
    FLUSH(ptr);
    FLUSHFENCE;
    return ptr;
  }

  static void __map_persistent_region(){
      int fd; 
      fd  = open(HEAP_FILE, O_RDWR | O_CREAT | O_TRUNC,
                    S_IRUSR | S_IWUSR);

      off_t offt = lseek(fd, REGION_SIZE-1, SEEK_SET);
      assert(offt != -1);

      int result = write(fd, "", 1); 
      assert(result != -1);

      void * addr =
          mmap(0, REGION_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
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
  static int __nvm_region_allocator(void** memptr, size_t alignment, size_t size)
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
      if (next > base_addr + REGION_SIZE){
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
  inline void pm_recover() { assert(0 && "not implemented"); }

  template<class T>
  inline T* pm_get_root(unsigned int i){
    return (T*)MAK_persistent_root(i);
  }
  inline void pm_set_root(void* ptr, unsigned int i) { return MAK_set_persistent_root(i, ptr); }

#elif defined(PMDK)

  // No longer support PMDK since it's too slow
  #include <libpmemobj.h>
  #ifdef SHM_SIMULATING
    #define HEAP_FILE "/dev/shm/pmdk_heap_wcai6"
  #else
    #define HEAP_FILE "/mnt/pmem/pmdk_heap_wcai6"
  #endif
  extern PMEMobjpool* pop;
  extern PMEMoid root;
  struct PMDK_roots{
    void* roots[1024];
  };
  static int dummy_construct(PMEMobjpool *pop, void *ptr, void *arg){return 0;}
  inline void* pm_malloc(size_t s) {
    PMEMoid temp_ptr;
    int ret=pmemobj_alloc(pop, &temp_ptr, s, 0, dummy_construct,nullptr);
    if(ret==-1)return nullptr;
    return pmemobj_direct(temp_ptr);
  }
  inline void pm_free(void* p) {
    if(p==nullptr) return;
    PMEMoid temp_ptr;
    temp_ptr = pmemobj_oid(p);
    pmemobj_free(&temp_ptr);
  }

  inline void* pm_realloc(void* ptr, size_t new_size) { 
    if(ptr==nullptr) return pm_malloc(new_size);
    PMEMoid temp_ptr;
    temp_ptr = pmemobj_oid(ptr);
    pmemobj_realloc(pop, &temp_ptr, new_size, 0);
    return pmemobj_direct(temp_ptr);
  }

  inline void* pm_calloc(size_t num, size_t size) { 
    void* ptr = pm_malloc(num*size);
    if(ptr == nullptr) return nullptr;
    memset(ptr, 0, num*size);
    FLUSH(ptr);
    FLUSHFENCE;
    return ptr;
  }

  inline int pm_init() {
    pop = pmemobj_create(HEAP_FILE, "test", REGION_SIZE, 0666);
    if (pop == nullptr) {
      perror("pmemobj_create");
      return 1;
    }
    else {
      root = pmemobj_root(pop, sizeof (PMDK_roots));
      return 0;
    }
  }
  inline void pm_close() {
      pmemobj_close(pop);
  }
  inline void pm_recover() { assert(0 && "not implemented"); }
  template<class T>
  inline T* pm_get_root(unsigned int i){
    return (T*)((PMDK_roots*)pmemobj_direct(root))->roots[i];
  }
  inline void pm_set_root(void* ptr, unsigned int i) { ((PMDK_roots*)pmemobj_direct(root))->roots[i] = ptr; }

#else // MAKALU ends

  extern void* roots[1024];
  inline void* pm_malloc(size_t s) { return malloc(s); }
  inline void pm_free(void* p) { free(p);}
  inline void* pm_realloc(void* ptr, size_t new_size) { return realloc(ptr, new_size); }
  inline void* pm_calloc(size_t num, size_t size) { return calloc(num, size); }
  inline int pm_init() { return 0; }
  inline void pm_close() { return; }
  inline void pm_recover() { assert(0 && "not implemented"); }
  template<class T>
  inline T* pm_get_root(unsigned int i){
    return (T*)roots[i];
  }
  inline void pm_set_root(void* ptr, unsigned int i) { roots[i] = ptr; }

#endif //else ends

#endif
