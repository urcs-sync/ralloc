/*
 * Copyright (C) 2019 University of Rochester. All rights reserved.
 * Licenced under the MIT licence. See LICENSE file in the project root for
 * details. 
 */

/*
 * Copyright (C) 2018 Ricardo Leite
 * Licenced under the MIT licence. This file shares some portion from
 * LRMalloc(https://github.com/ricleite/lrmalloc) and its copyright 
 * is retained. See LICENSE for details about MIT License.
 */

#ifndef _BASE_META_HPP_
#define _BASE_META_HPP_

#include <atomic>
#include <iostream>
#include <functional>
#include <set>
#include <vector>
#include <stack>
#include <utility>
#include <pthread.h>

#include "pm_config.hpp"

#include "RegionManager.hpp"
#include "SizeClass.hpp"
#include "TCache.hpp"
#include "pptr.hpp"

/********class BaseMeta********
 * This is the file where meta data structures of
 * ralloc are defined.
 *
 * The using logic is: 
 *  1.  Use RegionManager to allocate a persistent 
 *      region with size of BaseMeta
 *  2.  Cast the pointer to the persistent region 
 *      to BaseMeta*
 *  3.  Initialize BaseMeta* by calling constructor 
 *      in place.
 *
 * Some useful functions to know:
 *      void BaseMeta::restart():
 *          Restart the BaseMeta after data is remapped in
 *          BaseMeta. It traces and recovers free lists.
 *      void BaseMeta::writeback():
 *          Do writeback before program exits.
 *          It flushes free lists and caches and marks segment as clean.
 *      void* set_root(void* ptr, uint64_t i):
 *          Set persistent root i to ptr, return the old root
 *          in root i if there is, or nullptr if there isn't.
 *      void* get_root<T>(uint64_t i):
 *          Return persistent root i, or nullptr if there isn't.
 *          Type T is recorded as the type of root i and will be used in GC.
 *
 * Most of functions related to malloc and free share some portion of 
 * code with the open source project https://github.com/ricleite/lrmalloc
 * Some modifications were applied for bug fixing or functionality adjustment.
 *
 * Adapted and reimplemented by:
 *      Wentao Cai (wcai6@cs.rochester.edu)
 */

class BaseMeta;
namespace ralloc{
    /* manager to map, remap, and unmap the heap */
    // regions manager
    extern Regions* _rgs;//initialized when ralloc constructs
    // flag indicating Ralloc is initialized or not.
    extern bool initialized;
    // pointer to the instance of BaseMeta
    extern BaseMeta* base_md;
    // function to flush thread-local cache, used in TCaches::~TCaches and
    // BaseMeta::writeback()
    extern void public_flush_cache();
};

/* 
 * class CrossPtr<T, idx>
 *  
 * Description: 
 *  Pointers to the block in region idx.
 *  Relative address stored in CrossPtr is the offset from the start of that 
 *  region to the block.
 * 
 * Usage:
 *  T: the type of the pointer
 *  idx: the destination region index (RegionIndex defined in pm_config.hpp)
 */
template<class T, RegionIndex idx>
class CrossPtr {
public:
    char* off;
    CrossPtr(T* real_ptr = nullptr) noexcept;
    CrossPtr(const CrossPtr& cptr) noexcept: off(cptr.off) {}

    template<class F>
    inline operator F*() const{// cast to absolute pointer
        if(UNLIKELY(is_null())){
            return nullptr;
        } else{
            return reinterpret_cast<F*>(ralloc::_rgs->translate(idx, off));
        }
    } 
    T& operator* ();
    T* operator-> ();
    inline CrossPtr& operator= (const CrossPtr<T,idx> &p){
        off = p.off;
        return *this;
    }
    template<class F>
    inline CrossPtr& operator= (const F* p){
        uint64_t tmp = reinterpret_cast<uint64_t>(p);//get rid of const
        off = ralloc::_rgs->untranslate(idx, reinterpret_cast<char*>(tmp));
        return *this;
    }
    inline CrossPtr& operator= (const std::nullptr_t& p){
        off = nullptr; return *this;
    }
    inline bool is_null() const{
        return off == nullptr;
    }
};

template<class T, RegionIndex idx>
inline bool operator==(const CrossPtr<T,idx>& lhs, const std::nullptr_t& rhs){
    return lhs.is_null();
}

template<class T, RegionIndex idx>
inline bool operator==(const CrossPtr<T,idx>& lhs, const CrossPtr<T,idx>& rhs){
    return static_cast<T*>(lhs) == static_cast<T*>(rhs);
}

template <class T, RegionIndex idx>
inline bool operator!=(const CrossPtr<T,idx>& lhs, const std::nullptr_t& rhs){
    return !lhs.is_null();
}

/* 
 * class ptr_cnt<T>
 * 
 * Description:
 * This is a wrapper for plain pointer, rather than pptr.
 * Given that we don't use this in atomic (i.e., no atomic<ptr_cnt<T>>), we
 * don't bother fit both pointer and counter into 64 bits.
 * 
 * This class is to store the intermediate value of operations on 
 * AtomicCrossPtrCnt<T>, including atomic load, store, 
 * and CAS.
 */
template <class T>
class ptr_cnt{
public:
	T* ptr;//ptr
	uint64_t cnt;
	ptr_cnt(T* p=nullptr, uint64_t c = 0) noexcept:
		ptr(p), cnt(c){};
	void set(T* p, uint64_t c){
		ptr = p;
		cnt = c;
	}
	T* get_ptr() const{
		return ptr;
	}
	uint64_t get_counter() const{
		return cnt;
	}
};

/* 
 * class AtomicCrossPtrCnt<T, idx>
 *  
 * Description: 
 *  Atomic version of CrossPtr, where higher 34 bits is a counter to avoid ABA.
 * 
 * Usage:
 *  T: the type of the pointer
 *  idx: the destination region index (RegionIndex defined in pm_config.hpp)
 */
template<class T, RegionIndex idx>
class AtomicCrossPtrCnt {
public:
    std::atomic<char*> off; // higher 34 bits: counter, lower 30 bits: offset
    AtomicCrossPtrCnt(T* real_ptr = nullptr, uint64_t counter = 0) noexcept;
    ptr_cnt<T> load(std::memory_order order = std::memory_order_seq_cst) const noexcept;
    void store(ptr_cnt<T> desired, 
        std::memory_order order = std::memory_order_seq_cst ) noexcept;
    bool compare_exchange_weak(ptr_cnt<T>& expected, ptr_cnt<T> desired,
        std::memory_order order = std::memory_order_seq_cst ) noexcept;
    bool compare_exchange_strong(ptr_cnt<T>& expected, ptr_cnt<T> desired,
        std::memory_order order = std::memory_order_seq_cst ) noexcept;
};

/* 
 * enum SuperblockState
 * 
 * Description:
 *  Superblock states used in Anchor::state.
 */
enum SuperblockState {
    // invalid state
    SB_ERROR	= 0,
    // all blocks allocated or reserved
    SB_FULL		= 1,
    // has unreserved available blocks
    SB_PARTIAL	= 2,
    // all blocks are free
    SB_EMPTY	= 3,
};

struct Anchor;
struct DescriptorNode;
struct Descriptor;
struct ProcHeap;

/***************************************
 *** core data structures start here ***
 ***************************************/

/* 
 * struct Anchor
 * 
 * Description:
 *  64-bit anchor in each descriptor, descripting status of a superblock.
 */
struct Anchor{
    uint64_t avail:31,count:31, state:2;
    Anchor(uint64_t a = 0) noexcept {(*(uint64_t*)this) = a;}
    Anchor(unsigned a, unsigned c, unsigned s) noexcept :
        avail(a),count(c),state(s){};
};
static_assert(sizeof(Anchor) == sizeof(uint64_t), "Invalid anchor size");

/* 
 * struct Descriptor
 * 
 * Description: 
 *  Cache-line aligned descriptor of a superblock.
 *  Descriptors are arranged in desc region and *never* freed
 */
struct Descriptor {
    // free superblocks are linked by their descriptors
    RP_TRANSIENT atomic_pptr<Descriptor> next_free;
    // used in partial descriptor list
    RP_TRANSIENT atomic_pptr<Descriptor> next_partial;
    // anchor; is reconstructed during recovery
    RP_TRANSIENT std::atomic<Anchor> anchor;

    RP_PERSIST CrossPtr<char, SB_IDX> superblock;
    RP_PERSIST CrossPtr<ProcHeap, META_IDX> heap;
    RP_PERSIST uint32_t block_size; // block size acquired from sc
    RP_PERSIST uint32_t maxcount; // block number acquired from sc
    Descriptor() noexcept :
        next_free(),
        next_partial(),
        anchor(),
        superblock(),
        heap(),
        block_size(),
        maxcount(){
            FLUSH(this);
            FLUSHFENCE;
        };
}__attribute__((aligned(CACHELINE_SIZE)));
static_assert(sizeof(Descriptor) == CACHELINE_SIZE, "Invalid Descriptor size");

/* 
 * struct ProcHeap
 * 
 * Descrition:
 *  Legacy struct to store the reference to partial list of a sizeclass.
 *  Can be merged into sizeclass but I'm too lazy to do so. :D
 */
struct ProcHeap {
public:
    // ptr to descriptor, head of partial descriptor list
    RP_TRANSIENT AtomicCrossPtrCnt<Descriptor, DESC_IDX> partial_list;
    /* size class index; never change after init
     * though it's tagged RP_PERSIST, in 1/sc scheme,
     * we don't have to flush it at all; it's fixed.
     */
    RP_PERSIST size_t sc_idx;
    // std::mutex lk;
    ProcHeap() noexcept :
        partial_list(){};
}__attribute__((aligned(CACHELINE_SIZE)));

/* 
 * class GarbageCollection
 * 
 * Descrition:
 *  A function class to do garbage collection during a dirty restart.
 *  Will be instantiated when BaseMeta::restart() is called and the segment is
 *  dirty.
 */
class GarbageCollection{
public:
    std::set<char*> marked_blk;
    std::stack<char*> to_filter_node;
    std::stack<std::function<void(char*,GarbageCollection& gc)>> to_filter_func;

    GarbageCollection():marked_blk(){};

    void operator() ();

    // return true if ptr is a valid and unmarked pointer, otherwise false
    template<class T>
    inline void mark_func(T* ptr){
        void* addr = reinterpret_cast<void*>(ptr);
        // Step 1: check if it's a valid pptr
        if(UNLIKELY(!ralloc::_rgs->in_range(SB_IDX, addr))) 
            return; // return if not in range
        auto res = marked_blk.find(reinterpret_cast<char*>(addr));
        if(res == marked_blk.end()){
            // Step 2: mark potential pptr
            marked_blk.insert(reinterpret_cast<char*>(addr));
            // Step 3: push ptr to stack
            to_filter_node.push(reinterpret_cast<char*>(addr));
            to_filter_func.push([](char* ptr,GarbageCollection& gc){
                    gc.filter_func(reinterpret_cast<T*>(ptr));
                });
        }
        return;
    }

    template<class T>
    inline void filter_func(T* ptr);
};

namespace ralloc{
    // (transient) filter functions for each root
    extern std::function<void(const CrossPtr<char, SB_IDX>&, GarbageCollection&)> roots_filter_func[MAX_ROOTS];
}

/*
 * class BaseMeta
 * 
 * Description:
 *  The core data structure in this file.
 *  Contains essential metadata for Ralloc, including:
 *      avail_sb: superblock free list 
 *      dirty_attr, dirty_mtx: dirty flag
 *      heaps: sizeclasses and their partial lists
 *      roots: pointers to persistent roots
 *  do_malloc() and do_free() are the real entry point of Ralloc's malloc and
 *  free routines.
 */
class BaseMeta {
public:
    // unused small sb
    RP_TRANSIENT AtomicCrossPtrCnt<Descriptor, DESC_IDX> avail_sb;
    RP_PERSIST pthread_mutexattr_t dirty_attr;
    RP_PERSIST pthread_mutex_t dirty_mtx;

    RP_PERSIST ProcHeap heaps[MAX_SZ_IDX];
    RP_PERSIST CrossPtr<char, SB_IDX> roots[MAX_ROOTS];
    friend class GarbageCollection;
    BaseMeta() noexcept;
    ~BaseMeta(){
        /* usually BaseMeta shouldn't be destructed, 
         * and will be reused in the next time
         */
        std::cout<<"Warning: BaseMeta is being destructed!\n";
    }
    void* do_malloc(size_t size);
    void do_free(void* ptr);
    bool is_dirty();
    // set_dirty must be called AFTER is_dirty
    void set_dirty();
    void set_clean();
    inline uint64_t min(uint64_t a, uint64_t b){return a>b?b:a;}
    inline uint64_t max(uint64_t a, uint64_t b){return a>b?a:b;}
    inline uint64_t round_up(uint64_t numToRound, uint64_t multiple) {
        //only works for some multiple that is a power of 2
        assert(multiple && ((multiple & (multiple - 1)) == 0));
        return (numToRound + multiple - 1) & ~(multiple - 1);
    }
    void* set_root(void* ptr, uint64_t i){
        //this is sequential
        assert(i<MAX_ROOTS);
        void* res = nullptr;
        if(roots[i]!=nullptr) 
            res = static_cast<void*>(roots[i]);
        roots[i] = ptr;

        FLUSH(&roots[i]);
        FLUSHFENCE;
        return res;
    }
    template<class T>
    inline T* get_root(uint64_t i){
        //this is sequential
        // assert(i<MAX_ROOTS && roots[i]!=nullptr); // we allow roots[i] to be null
        assert(i<MAX_ROOTS);
        ralloc::roots_filter_func[i] = [](const CrossPtr<char, SB_IDX>& cptr, GarbageCollection& gc){
            // this new statement is intentionally designed to use transient allocator since it's offline
            gc.mark_func(static_cast<T*>(cptr));
        };
        return static_cast<T*>(roots[i]);
    }
    bool restart(){
        // Restart, setting values and flags to normal
        // Should be called during restart
        bool ret = is_dirty();
        if(ret) {
            GarbageCollection gc;
            gc();
        }
        FLUSHFENCE;
        // here restart is done, and "dirty" should be set to true until
        // writeback() is called so that crash will result in a true dirty.
        set_dirty();
        return ret;
    }
    void writeback(){
        // Give back tcached blocks *Wentao: no actually ~TCache will do this*
        // Should be called during normal exit
        // ralloc::public_flush_cache();
        char* addr = reinterpret_cast<char*>(this);
        // flush values in BaseMeta, including avail_sb and partial lists
        for(size_t i = 0; i < sizeof(BaseMeta); i += CACHELINE_SIZE) {
            addr += CACHELINE_SIZE;
            FLUSH(addr);
        }
        FLUSHFENCE;
        set_clean();
    }

private:
    // void* expand_sb(size_t sz);
    // void expand_small_sb();
    // void* expand_get_small_sb();
    void* expand_get_large_sb(size_t sz);

    // func on size class
    size_t get_sizeclass(size_t size);
    SizeClassData* get_sizeclass(ProcHeap* h);
    SizeClassData* get_sizeclass_by_idx(size_t idx);
    // compute block index in superblock by addr to sb, block, and sc index
    uint32_t compute_idx(char* superblock, char* block, size_t sc_idx);

    // func on cache
    void fill_cache(size_t sc_idx, TCacheBin* cache);
public:
    // we need to call this function to flush TLS cache during exit
    void flush_cache(size_t sc_idx, TCacheBin* cache);
    // find desc of the block
    // we need to call them in GC
    Descriptor* desc_lookup(const char* ptr);
    inline Descriptor* desc_lookup(const void* ptr){return desc_lookup(reinterpret_cast<const char*>(ptr));}
    char* sb_lookup(Descriptor* desc);

private:
    // helper func
    void heap_push_partial(Descriptor* desc);
    Descriptor* heap_pop_partial(ProcHeap* heap);
    // fill cache from a partially used sb in heap[sc_idx]
    void malloc_from_partial(size_t sc_idx, TCacheBin* cache, size_t& block_num);
    // fill cache by allocating a new sb in heap[sc_idx]
    void malloc_from_newsb(size_t sc_idx, TCacheBin* cache, size_t& block_num);
    // alloc function to call for large block
    void* alloc_large_block(size_t sz);

    // add all newly allocated sbs to free_sb
    void organize_sb_list(void* start, uint64_t count);
    // get one free sb or allocate a new space for sbs
    void* small_sb_alloc(size_t size);
    // free the superblock sb points to
    void small_sb_retire(void* sb, size_t size);

    // allocate a large sb
    void* large_sb_alloc(size_t size);
    // retire a large sb
    void large_sb_retire(void* sb, size_t size);

    // get unused desc from avail_desc or allocate a new space for desc
    Descriptor* desc_alloc();
    // put desc to avail_desc and flush it as unused
    void desc_retire(Descriptor* desc);
}__attribute__((aligned(CACHELINE_SIZE)));

// default (conservative) filter function which traverse all possible pointers
// in the block
template<class T>
inline void GarbageCollection::filter_func(T* ptr){
    char* curr = reinterpret_cast<char*>(ptr);
    Descriptor* desc = ralloc::base_md->desc_lookup((char*)ptr);
    size_t sz = desc->block_size;
    for(size_t i=0;i<sz;i++){
        char* curr_content = static_cast<char*>(*(reinterpret_cast<pptr<char>*>(curr)));
        if(curr_content!=nullptr)
            mark_func(curr_content);
        curr++;
    }
}

#endif /* _BASE_META_HPP_ */
