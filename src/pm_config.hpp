#ifndef _PM_CONFIG_HPP_
#define _PM_CONFIG_HPP_


#include <assert.h>

#include "pfence_util.h"

/* prefixing indicator */
// persistent data in pmmalloc
#define PM_PERSIST
// transient data in pmmalloc
#define PM_TRANSIENT

#define LIKELY(x) __builtin_expect((x), 1)
#define UNLIKELY(x) __builtin_expect((x), 0)

// returns smallest value >= value with alignment align
#define ALIGN_VAL(val, align) \
    ( __typeof__ (val))(((uint64_t)(val) + (align - 1)) & ((~(align)) + 1))

// returns smallest address >= addr with alignment align
#define ALIGN_ADDR(addr, align) ALIGN_VAL(addr, align)

// return smallest page size multiple that is >= s
#define PAGE_CEILING(s) \
    (((s) + (PAGESIZE - 1)) & ~(PAGESIZE - 1))

#ifdef DEBUG
  #define DBG_PRINT(msg, ...) \
    fprintf(stderr, "%s:%d %s " msg "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    fflush(stderr);
#else
  #define DBG_PRINT(msg, ...)
#endif

/* user customized macros */
#define HEAPFILE_PREFIX "/dev/shm/"
#define ENABLE_FILTER_FUNC 1
// #define DEBUG 1
const uint64_t MAX_FILESIZE = 16*1024*1024*1024ULL;
const uint64_t MAX_THREADS = 512;
const int MAX_ROOTS = 1024;
const int MAX_SECTION = 128;
const uint64_t MAX_BLOCK_NUM = (2ULL << 31);//max number of blocks per sb
const int DESC_SPACE_CAP = 128;//number of desc sbs per desc space
const uint64_t SB_SPACE_SIZE = 1*1024*1024*1024ULL;
const int PAGESIZE = 4096;//4K
const uint64_t PAGE_MASK = (uint64_t)PAGESIZE - 1;
const int SBSIZE = (16 * PAGESIZE); // size of a superblock 64K
const int DESCSBSIZE = (1024 * 64);//assume sizeof(Descriptor) is 64
const int DESC_SPACE_SIZE = DESC_SPACE_CAP * DESCSBSIZE;
//Note: by this config, approximately 1 sb space needs 8 desc space

/* constant variables */
const int TYPE_SIZE = 4;
const int PTR_SIZE = sizeof(void*);
const int HEADER_SIZE = (TYPE_SIZE + PTR_SIZE);
const int CACHELINE_SIZE = 64;
const uint64_t CACHELINE_MASK = (uint64_t)(CACHELINE_SIZE) - 1;

const int LARGE = 249; // tag indicating the block is large
const int SMALL = 250; // tag indicating the block is small

// number of size classes; idx 0 reserved for large size classes
const int MAX_SZ_IDX = 40;
const uint64_t SC_MASK = (1ULL << 6) - 1;
// last size covered by a size class
// allocations with size > MAX_SZ are not covered by a size class
const int MAX_SZ = ((1 << 13) + (1 << 11) * 3);
const int PROCHEAP_NUM = MAX_THREADS; // number of processor heap

const uint64_t FREELIST_CAP = 10*1024*1024ULL; //largest amount of nodes in freelist
const uint64_t FREESTACK_CAP = FREELIST_CAP;
const uint64_t FREEQUEUE_CAP = FREELIST_CAP;
const uint64_t PARTIAL_CAP = 10*1024ULL;//largest amount of nodes in partial list
const int PPTR_PREFIX = 0X5AB0; // prefix for pptr offset. 7A as 'Z' and B0 as "BO", the name of my dearest ;)
#endif