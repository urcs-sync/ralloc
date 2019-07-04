#ifndef _RP_CONFIG_HPP_
#define _RP_CONFIG_HPP_


#include <assert.h>

#include "pfence_util.h"

/* prefixing indicator */
// persistent data in rpmalloc
#define RP_PERSIST
// transient data in rpmalloc
#define RP_TRANSIENT

// region index
enum RegionIndex {
	DESC_IDX = 0,
	SB_IDX = 1,
	META_IDX = 2,
	LAST_IDX // dummy index as the last
};

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

/* System Macros */
// OK I HATE macros but if I use const int then c compilers would complain
// C language really sxxks
#define TYPE_SIZE 4
#define PTR_SIZE sizeof(void*)
const int HEADER_SIZE = (TYPE_SIZE + PTR_SIZE);
// in byte
#define CACHELINE_SIZE 64
const uint64_t CACHELINE_MASK = (uint64_t)(CACHELINE_SIZE) - 1;
//4K
#define PAGESIZE 4096
const uint64_t PAGE_MASK = (uint64_t)PAGESIZE - 1;

/* Library Invariant */
const int LARGE = 249; // tag indicating the block is large
const int SMALL = 250; // tag indicating the block is small
// number of size classes; idx 0 reserved for large size classes
const int MAX_SZ_IDX = 40;
const uint64_t SC_MASK = (1ULL << 6) - 1;
// last size covered by a size class
// allocations with size > MAX_SZ are not covered by a size class
const int MAX_SZ = ((1 << 13) + (1 << 11) * 3);
// size of a superblock 64K
#define SBSIZE (16 * PAGESIZE)
#define DESCSIZE CACHELINE_SIZE
const int SB_SHIFT = 16; // assume size of a superblock is 64K
const int DESC_SHIFT = 6; // assume size of a descriptor is 64B

/* Customizable Values */
// maximum of superblocks in region
#define MAX_SB_AMOUNT (256*1024ULL)
#define MAX_DESC_AMOUNT MAX_SB_AMOUNT
const uint64_t MIN_SB_REGION_SIZE = SBSIZE*MAX_SB_AMOUNT; // min sb region size
const int64_t MAX_SB_REGION_SIZE = 64*SBSIZE*MAX_SB_AMOUNT; // max possible sb region size to call RP_init. Currently it's 1TB which must be sufficient
const uint64_t MAX_DESC_REGION_SIZE = DESCSIZE*MAX_DESC_AMOUNT;
const int MAX_ROOTS = 1024;
const uint64_t SB_REGION_EXPAND_SIZE = 1*1024*1024*1024ULL;
/*
 * Dig 16 least significant bits inside pptr and atomic_pptr to create unique 
 * bits pattern. The least bit is sign bit.
 */
const uint64_t PPTR_PATTERN_POS = 0x52b0;
const uint64_t PPTR_PATTERN_NEG = 0x52b1;
const int PPTR_PATTERN_SHIFT = 16;
#define PPTR_PATTERN_MASK ((1<<PPTR_PATTERN_SHIFT)-2)
#endif