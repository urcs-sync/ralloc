#ifndef ALLOCATOR_MACRO
#define ALLOCATOR_MACRO
#include "pfence_util.h"
#ifdef PMMALLOC

  #include "rpmalloc.hpp"
  inline void* pm_malloc(size_t s) { return RP_malloc(s); }
  inline void pm_free(void* p) { RP_free(p); }
  inline int pm_init() { return RP_init("test", 5*1024*1024*1024ULL + 24); }
  inline void pm_close() { RP_close(); }

#elif defined(MAKALU) // PMMALLOC ends

  #include "makalu.h"
  #include <fcntl.h>
  #include <sys/mman.h>
  #define MAKALU_FILESIZE (5*1024*1024*1024ULL + 24)
  inline void* pm_malloc(size_t s) { return MAK_malloc(s); }
  inline void pm_free(void* p) { MAK_free(p);}
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
  inline int pm_init() {
    __map_persistent_region();
    MAK_start(&__nvm_region_allocator);
    return 0;
  }

  inline void pm_close() { MAK_close(); }

#elif defined(PMDK)

  // No longer support PMDK since it's too slow
  #include <libpmemobj.h>
  #define HEAPFILE "/dev/shm/pmdk_heap_wcai6"
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
