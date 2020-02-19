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

#include <sys/mman.h>

#include <string>
#include <chrono> 
#include <iostream>

#include "BaseMeta.hpp"

/*
 * BaseMeta.cpp contains implementation of most types and functions declared in
 * BaseMeta.hpp, please find description of their functionality in BaseMeta.hpp.
 */
using namespace std;
using namespace ralloc;
using namespace std::chrono;
template<class T, RegionIndex idx>
CrossPtr<T,idx>::CrossPtr(T* real_ptr) noexcept{
    if(UNLIKELY(real_ptr == nullptr)){
        off = nullptr;
    } else {
        off = _rgs->untranslate(idx, reinterpret_cast<char*>(real_ptr));
    }
}

template<class T, RegionIndex idx>
T& CrossPtr<T,idx>::operator*(){
    return *(reinterpret_cast<T*>(_rgs->translate(idx, off)));
}

template<class T, RegionIndex idx>
T* CrossPtr<T,idx>::operator->(){
    return reinterpret_cast<T*>(_rgs->translate(idx, off));
}

template<class T, RegionIndex idx>
AtomicCrossPtrCnt<T,idx>::AtomicCrossPtrCnt(T* real_ptr, uint64_t counter)noexcept{
    char* res;
    uint64_t cnt_prefix = counter << MAX_DESC_OFFSET_BITS;
    if(real_ptr == nullptr) {
        res = reinterpret_cast<char*>(cnt_prefix);
    } else {
        res = _rgs->untranslate(idx, reinterpret_cast<char*>(real_ptr));
        res = reinterpret_cast<char*>(reinterpret_cast<uint64_t>(res) | cnt_prefix);
    }
    off.store(res);
}

// wrapped-up atomic ops for AtomicCrossPtrCnt
template<class T, RegionIndex idx>
inline ptr_cnt<T> AtomicCrossPtrCnt<T,idx>::load(memory_order order)const noexcept{
    char* cur_off = off.load(order);
    ptr_cnt<T> ret;
    ret.cnt = reinterpret_cast<uint64_t>(cur_off) >> MAX_DESC_OFFSET_BITS;
    cur_off = reinterpret_cast<char*>(reinterpret_cast<uint64_t>(cur_off) & MAX_DESC_OFFSET_MASK);
    if(cur_off == nullptr){
        ret.ptr = nullptr;
    } else{
        ret.ptr = (T*)(_rgs->translate(idx, cur_off));
    }
    return ret;
}

template<class T, RegionIndex idx>
inline void AtomicCrossPtrCnt<T,idx>::store(ptr_cnt<T> desired, memory_order order)noexcept{
    char* new_off;
    uint64_t cnt_prefix = desired.cnt << MAX_DESC_OFFSET_BITS;
    if(desired.get_ptr() == nullptr){
        new_off = reinterpret_cast<char*>(cnt_prefix);
    } else{
        new_off = _rgs->untranslate(idx, reinterpret_cast<char*>(desired.ptr));
        new_off = reinterpret_cast<char*>(reinterpret_cast<uint64_t>(new_off) | cnt_prefix);
    }
    off.store(new_off, order);
}

template<class T, RegionIndex idx>
inline bool AtomicCrossPtrCnt<T,idx>::compare_exchange_weak(ptr_cnt<T>& expected, ptr_cnt<T> desired, memory_order order)noexcept{
    char* old_off;
    char* new_off;
    uint64_t old_cnt_prefix = expected.cnt << MAX_DESC_OFFSET_BITS;
    uint64_t new_cnt_prefix = desired.cnt << MAX_DESC_OFFSET_BITS;
    if(expected.get_ptr() == nullptr){
        old_off = reinterpret_cast<char*>(old_cnt_prefix);
    } else {
        old_off = _rgs->untranslate(idx, reinterpret_cast<char*>(expected.ptr));
        old_off = reinterpret_cast<char*>(reinterpret_cast<uint64_t>(old_off) | old_cnt_prefix);
    }
    if(desired.get_ptr() == nullptr){
        new_off = reinterpret_cast<char*>(new_cnt_prefix);
    } else {
         new_off = _rgs->untranslate(idx, reinterpret_cast<char*>(desired.ptr));
        new_off = reinterpret_cast<char*>(reinterpret_cast<uint64_t>(new_off) | new_cnt_prefix);
    }
    bool ret = off.compare_exchange_weak(old_off, new_off, order);
    if(!ret){
        expected.cnt = reinterpret_cast<uint64_t>(old_off) >> MAX_DESC_OFFSET_BITS;
        old_off = reinterpret_cast<char*>(reinterpret_cast<uint64_t>(old_off) & MAX_DESC_OFFSET_MASK);
        if(old_off == nullptr){
            expected.ptr = nullptr;
        } else{
            expected.ptr = (T*)(_rgs->translate(idx, old_off));
        }
    }
    return ret;
}

template<class T, RegionIndex idx>
inline bool AtomicCrossPtrCnt<T,idx>::compare_exchange_strong(ptr_cnt<T>& expected, ptr_cnt<T> desired, memory_order order)noexcept{
    char* old_off;
    char* new_off;
    uint64_t old_cnt_prefix = expected.cnt << MAX_DESC_OFFSET_BITS;
    uint64_t new_cnt_prefix = desired.cnt << MAX_DESC_OFFSET_BITS;
    if(expected.get_ptr() == nullptr){
        old_off = reinterpret_cast<char*>(old_cnt_prefix);
    } else {
        old_off = _rgs->untranslate(idx, reinterpret_cast<char*>(expected.ptr));
        old_off = reinterpret_cast<char*>(reinterpret_cast<uint64_t>(old_off) | old_cnt_prefix);
    }
    if(desired.get_ptr() == nullptr){
        new_off = reinterpret_cast<char*>(new_cnt_prefix);
    } else {
         new_off = _rgs->untranslate(idx, reinterpret_cast<char*>(desired.ptr));
        new_off = reinterpret_cast<char*>(reinterpret_cast<uint64_t>(new_off) | new_cnt_prefix);
    }
    bool ret = off.compare_exchange_strong(old_off, new_off, order);
    if(!ret){
        expected.cnt = reinterpret_cast<uint64_t>(old_off) >> MAX_DESC_OFFSET_BITS;
        old_off = reinterpret_cast<char*>(reinterpret_cast<uint64_t>(old_off) & MAX_DESC_OFFSET_MASK);
        if(old_off == nullptr){
            expected.ptr = nullptr;
        } else{
            expected.ptr = (T*)(_rgs->translate(idx, old_off));
        }
    }
    return ret;
}

void BaseMeta::set_dirty(){
    // this must be called AFTER is_dirty
    int s = pthread_mutex_trylock(&dirty_mtx);
    assert(s!=EOWNERDEAD&&"previous apps died! call is_dirty first!");
}

bool BaseMeta::is_dirty(){
    int s = pthread_mutex_trylock(&dirty_mtx);
    switch(s){
    case EOWNERDEAD:
        pthread_mutex_consistent(&dirty_mtx);
        return true;
    case 0:
        // succeeds
        pthread_mutex_unlock(&dirty_mtx);
        return false;
    case EBUSY:
    case EAGAIN:
        return false;
    case EINVAL:
        pthread_mutex_destroy(&dirty_mtx);
        pthread_mutex_init(&dirty_mtx, &dirty_attr);
        return true;
    default:
        printf("something unexpected happens when check dirty_mtx\n"); 
        exit(1);
    }// switch(s)
}

void BaseMeta::set_clean(){
    pthread_mutex_unlock(&dirty_mtx);
}

BaseMeta::BaseMeta() noexcept
: 
    avail_sb(),
    heaps()
    // thread_num(thd_num) {
{
    /* allocate these persistent data into specific memory address */
    pthread_mutexattr_init(&dirty_attr);
    pthread_mutexattr_setrobust(&dirty_attr, PTHREAD_MUTEX_ROBUST);
    pthread_mutex_init(&dirty_mtx, &dirty_attr);
    set_dirty();
    FLUSH(&dirty_attr);
    FLUSH(&dirty_mtx);
    /* heaps init */
    for (size_t idx = 0; idx < MAX_SZ_IDX; ++idx){
        ProcHeap& heap = heaps[idx];
        heap.partial_list.store(nullptr);
        heap.sc_idx = idx;
        FLUSH(&heaps[idx]);
    }

    /* persistent roots init */
    for(int i=0;i<MAX_ROOTS;i++){
        roots[i]=nullptr;
        FLUSH(&roots[i]);
    }

    // warm up small sb space, expanding sb region by SB_REGION_EXPAND_SIZE
    void* tmp_sec_start = nullptr;
    int res = 0;
    while (res == 0){
        res = _rgs->expand(SB_IDX,&tmp_sec_start,SBSIZE, SB_REGION_EXPAND_SIZE);
        assert(res != -1 && "warmup sb allocation fails!");
    }
    DBG_PRINT("expand sb space for small sb allocation\n");
    _rgs->regions[SB_IDX]->__store_heap_start(tmp_sec_start);
    _rgs->regions_address[SB_IDX] = (char*)tmp_sec_start;
    //we skip the first sb on purpose so that CrossPtr doesn't start from 0.
    tmp_sec_start = (char*)((uint64_t)tmp_sec_start+SBSIZE);
    organize_sb_list(tmp_sec_start, SB_REGION_EXPAND_SIZE/SBSIZE-1);
    FLUSHFENCE;
}

// inline void* BaseMeta::expand_sb(size_t sz){
//     void* tmp_sec_start = nullptr;
//     bool res = _rgs->expand(SB_IDX,&tmp_sec_start,PAGESIZE, sz);
//     if(!res) assert(0&&"region allocation fails!");
//     return tmp_sec_start;
// }

//desc of returned sb is constructed
// inline void* BaseMeta::expand_get_small_sb(){
//     void* tmp_sec_start = nullptr;
//     int res = 0;
//     while(res == 0) {
//         res = _rgs->expand(SB_IDX,&tmp_sec_start,PAGESIZE, SB_REGION_EXPAND_SIZE);
//         assert(res != -1 && "space runs out!");
//     }
//     DBG_PRINT("expand sb space for small sb allocation\n");
//     organize_sb_list((char*)((uint64_t)tmp_sec_start+SBSIZE), SB_REGION_EXPAND_SIZE/SBSIZE-1);
//     Descriptor* desc = desc_lookup(tmp_sec_start);
//     new (desc) Descriptor();
//     return tmp_sec_start;
// }

//desc of returned sb is constructed
inline void* BaseMeta::expand_get_large_sb(size_t sz){
    void* ret = nullptr;
    int res = 0;
    while(res == 0) {
        res = _rgs->expand(SB_IDX,&ret,PAGESIZE, sz);
        assert(res != -1 && "space runs out!");
    }
    DBG_PRINT("expand sb space for large sb allocation\n");
    
    Descriptor* desc = desc_lookup(ret);
    new (desc) Descriptor();
    return ret;
}

inline size_t BaseMeta::get_sizeclass(size_t size){
    return sizeclass.get_sizeclass(size);
}

inline SizeClassData* BaseMeta::get_sizeclass(ProcHeap* h){
    return get_sizeclass_by_idx(h->sc_idx);
}

inline SizeClassData* BaseMeta::get_sizeclass_by_idx(size_t idx) { 
    return sizeclass.get_sizeclass_by_idx(idx);
}

uint32_t BaseMeta::compute_idx(char* superblock, char* block, size_t sc_idx) {
    SizeClassData* sc = get_sizeclass_by_idx(sc_idx);
    uint32_t sc_block_size = sc->block_size;
    (void)sc_block_size; // suppress unused var warning

    assert(block >= superblock);
    assert(block < superblock + sc->sb_size);
    // optimize integer division by allowing the compiler to create 
    //  a jump table using size class index
    // compiler can then optimize integer div due to known divisor
    uint32_t diff = uint32_t(block - superblock);
    uint32_t idx = 0;
    switch (sc_idx) {
#define SIZE_CLASS_bin_yes(index, block_size)		\
        case index:									\
            assert(sc_block_size == block_size);	\
            idx = diff / block_size;				\
            break;
#define SIZE_CLASS_bin_no(index, block_size)
#define SC(index, lg_grp, lg_delta, ndelta, psz, bin, pgs, lg_delta_lookup) \
        SIZE_CLASS_bin_##bin((index + 1), ((1U << lg_grp) + (ndelta << lg_delta)))
        SIZE_CLASSES
        default:
            assert(false);
            break;
    }
#undef SIZE_CLASS_bin_yes
#undef SIZE_CLASS_bin_no
#undef SC

    assert(diff / sc_block_size == idx);
    return idx;
}

void BaseMeta::fill_cache(size_t sc_idx, TCacheBin* cache) {
    // at most cache will be filled with number of blocks equal to superblock
    size_t block_num = 0;
    // use a *SINGLE* partial superblock to try to fill cache
    malloc_from_partial(sc_idx, cache, block_num);
    // if we obtain no blocks from partial superblocks, create a new superblock
    if (block_num == 0)
        malloc_from_newsb(sc_idx, cache, block_num);

    SizeClassData* sc = get_sizeclass_by_idx(sc_idx);
    (void)sc;
    assert(block_num > 0);
    assert(block_num <= sc->cache_block_num);
}

void BaseMeta::flush_cache(size_t sc_idx, TCacheBin* cache) {
    ProcHeap* heap = &heaps[sc_idx];
    SizeClassData* sc = get_sizeclass_by_idx(sc_idx);
    uint32_t const sb_size = sc->sb_size;
    uint32_t const block_size = sc->block_size;
    // after CAS, desc might become empty and
    //  concurrently reused, so store maxcount
    uint32_t const maxcount = sc->get_block_num();
    (void)maxcount; // suppress unused warning

    // @todo: optimize
    // in the normal case, we should be able to return several
    //  blocks with a single CAS
    while (cache->get_block_num() > 0) {
        char* head = cache->peek_block();
        char* tail = head;
        Descriptor* desc = desc_lookup(head);
        char* superblock = static_cast<char*>(desc->superblock);

        // cache is a linked list of blocks
        // superblock free list is also a linked list of blocks
        // can optimize transfers of blocks between these 2 entities
        //  by exploiting existing structure
        uint32_t block_count = 1;
        // check if next cache blocks are in the same superblock
        // same superblock, same descriptor
        while (cache->get_block_num() > block_count) {
            char* ptr = static_cast<char*>(*(pptr<char>*)tail);
            if (ptr < superblock || ptr >= superblock + sb_size)
                break; // ptr not in superblock

            // ptr in superblock, add to "list"
            ++block_count;
            tail = ptr;
        }

        cache->pop_list(static_cast<char*>(*(pptr<char>*)tail), block_count);

        // add list to desc, update anchor
        uint32_t idx = compute_idx(superblock, head, sc_idx);

        Anchor oldanchor = desc->anchor.load();
        Anchor newanchor;
        do {
            // update anchor.avail
            char* next = (char*)(superblock + oldanchor.avail * block_size);
            *(pptr<char>*)tail = next;

            newanchor = oldanchor;
            newanchor.avail = idx;
            // state updates
            // don't set SB_PARTIAL if state == SB_ACTIVE
            if (oldanchor.state == SB_FULL)
                newanchor.state = SB_PARTIAL;
            // this can't happen with SB_ACTIVE
            // because of reserved blocks
            assert(oldanchor.count < desc->maxcount);
            if (oldanchor.count + block_count == desc->maxcount) {
                newanchor.count = desc->maxcount - 1;
                newanchor.state = SB_EMPTY; // can free superblock
            }
            else
                newanchor.count += block_count;
        }
        while (!desc->anchor.compare_exchange_weak(oldanchor, newanchor));

        // after last CAS, can't reliably read any desc fields
        // as desc might have become empty and been concurrently reused
        assert(oldanchor.avail < maxcount || oldanchor.state == SB_FULL);
        assert(newanchor.avail < maxcount);
        assert(newanchor.count < maxcount);

        // CAS success
        if (oldanchor.state == SB_FULL) {
            if(newanchor.state == SB_EMPTY) {
                // this sb becomes empty from full
                small_sb_retire(superblock, SBSIZE);
            } else {
                // this sb becomes partial from full
                heap_push_partial(desc);
            }
        }
    }
}

Descriptor* BaseMeta::desc_lookup(const char* ptr){
    uint64_t sb_index = (((uint64_t)ptr)>>SB_SHIFT) - (((uint64_t)_rgs->lookup(SB_IDX))>>SB_SHIFT); // the index of sb this block in
    Descriptor* ret = reinterpret_cast<Descriptor*>(_rgs->lookup(DESC_IDX));
    ret+=sb_index;
    return ret;
}

char* BaseMeta::sb_lookup(Descriptor* desc){
    uint64_t desc_index = (((uint64_t)desc)>>DESC_SHIFT) - (((uint64_t)_rgs->lookup(DESC_IDX))>>DESC_SHIFT); // the index of sb this block in
    char* ret = _rgs->lookup(SB_IDX);
    ret = reinterpret_cast<char*>(((uint64_t)ret) + (desc_index<<SB_SHIFT)); // start of sb region + desc_index*SBSIZE
    return ret;
}

void BaseMeta::heap_push_partial(Descriptor* desc) {
    ProcHeap* heap = desc->heap;
    ptr_cnt<Descriptor> oldhead = heap->partial_list.load();
    ptr_cnt<Descriptor> newhead;
    do {
        newhead.set(desc, oldhead.get_counter() + 1);
        assert(oldhead.get_ptr() != newhead.get_ptr());
        newhead.get_ptr()->next_partial.store(oldhead.get_ptr()); 
    } while (!heap->partial_list.compare_exchange_weak(oldhead, newhead));
}

Descriptor* BaseMeta::heap_pop_partial(ProcHeap* heap) {
    ptr_cnt<Descriptor> oldhead = heap->partial_list.load();
    ptr_cnt<Descriptor> newhead;
    do {
        Descriptor* olddesc = oldhead.get_ptr();
        if (!olddesc){
            return nullptr;
        }
        Descriptor* desc = olddesc->next_partial.load();
        uint64_t counter = oldhead.get_counter();
        newhead.set(desc, counter);
    } while (!heap->partial_list.compare_exchange_weak(oldhead, newhead));
    return oldhead.get_ptr();
}

void BaseMeta::malloc_from_partial(size_t sc_idx, TCacheBin* cache, size_t& block_num){
retry:
    ProcHeap* heap = &heaps[sc_idx];

    Descriptor* desc = heap_pop_partial(heap);
    if (!desc)
        return;

    // reserve block(s)
    Anchor oldanchor = desc->anchor.load();
    Anchor newanchor;
    uint32_t maxcount = desc->maxcount;
    uint32_t block_size = desc->block_size;
    char* superblock = desc->superblock;

    // we have "ownership" of block, but anchor can still change
    // due to free()
    do {
        if (oldanchor.state == SB_EMPTY) {
            small_sb_retire(superblock, get_sizeclass(heap)->sb_size);
            goto retry;
        }

        // oldanchor must be SB_PARTIAL
        // can't be SB_FULL because we *own* the block now
        // and it came from heap_pop_partial
        // can't be SB_EMPTY, we already checked
        // obviously can't be SB_ACTIVE
        assert(oldanchor.state == SB_PARTIAL);

        newanchor = oldanchor;
        newanchor.count = 0;
        // avail value doesn't actually matter
        newanchor.avail = maxcount;
        newanchor.state = SB_FULL;
    }
    while (!desc->anchor.compare_exchange_weak(
                oldanchor, newanchor));

    // will take as many blocks as available from superblock
    // *AND* no thread can do malloc() using this superblock, we
    //  exclusively own it
    // if CAS fails, it just means another thread added more available blocks
    //  through FlushCache, which we can then use
    uint32_t block_take = oldanchor.count;
    uint32_t avail = oldanchor.avail;

    assert(avail < maxcount);
    char* block = superblock + avail * block_size;

    // cache must be empty at this point
    // and the blocks are already organized as a list
    // so all we need do is "push" that list, a constant time op
    assert(cache->get_block_num() == 0);
    cache->push_list(block, block_take);

    block_num += block_take;
}

void BaseMeta::malloc_from_newsb(size_t sc_idx, TCacheBin* cache, size_t& block_num) {
    ProcHeap* heap = &heaps[sc_idx];
    SizeClassData* sc = get_sizeclass_by_idx(sc_idx);
    uint32_t const block_size = sc->block_size;
    uint32_t const maxcount = sc->get_block_num();

    char* superblock = reinterpret_cast<char*>(small_sb_alloc(sc->sb_size));
    assert(superblock);
    Descriptor* desc = desc_lookup(superblock);

    desc->heap = heap;
    desc->block_size = block_size;
    desc->maxcount = maxcount;
    desc->superblock = superblock;

    // prepare block list
    for (uint32_t idx = 0; idx < maxcount - 1; ++idx) {
        pptr<char>* block = (pptr<char>*)(superblock + idx * block_size);
        char* next = superblock + (idx + 1) * block_size;
        *block = next;
    }

    // push blocks to thread local cache
    char* block = superblock; // first block
    cache->push_list(block, maxcount);

    Anchor anchor;
    anchor.avail = maxcount;
    anchor.count = 0;
    anchor.state = SB_FULL;
    desc->anchor.store(anchor);

    FLUSH(desc);
    FLUSHFENCE;

    assert(anchor.avail < maxcount || anchor.state == SB_FULL);
    assert(anchor.count < maxcount);

    // if state changes to SB_PARTIAL, desc must be added to partial list
    assert(anchor.state == SB_FULL);

    block_num += maxcount;
}

//for sb in the free list, their desc are all constructed.
inline void BaseMeta::organize_sb_list(void* start, uint64_t count){
    // put (start)...(start+count-1) sbs to free_sb queue
    // in total it's count sbs
    Descriptor* desc_start = desc_lookup((char*)((uint64_t)start));
    Descriptor* desc = desc_start;
    new (desc) Descriptor();
    for(uint64_t i = 1; i < count; i++){
        desc->next_free.store(desc+1);//pptr
        desc++;
        new (desc) Descriptor();
    }
    ptr_cnt<Descriptor> oldhead = avail_sb.load();
    ptr_cnt<Descriptor> newhead;
    do{
        desc->next_free.store(oldhead.get_ptr());
        newhead.set(desc_start, oldhead.get_counter()+1);
    }while(!avail_sb.compare_exchange_weak(oldhead,newhead));
}

void* BaseMeta::small_sb_alloc(size_t size){
    if(size != SBSIZE){
        std::cout<<"desired size: "<<size<<std::endl;
        assert(0);
    }

    Descriptor* oldptr = nullptr;

    ptr_cnt<Descriptor> oldhead = avail_sb.load();
    while(true){
        oldptr = oldhead.get_ptr();
        if(oldptr) {
            ptr_cnt<Descriptor> newhead;
            newhead.set(oldptr->next_free.load(),oldhead.get_counter());
            if(avail_sb.compare_exchange_strong(oldhead,newhead)){
                return reinterpret_cast<void*>(sb_lookup(oldptr));
            }
        }
        else{
            // below is effectively _rgs->regions[SB_IDX](&tmp_sec_start,PAGESIZE, SB_REGION_EXPAND_SIZE);
            char* next;
            char* res = nullptr;
            char * old_curr_addr = _rgs->regions[SB_IDX]->curr_addr_ptr->load();
            char * new_curr_addr = old_curr_addr;
            size_t aln_adj = (size_t) new_curr_addr & (PAGESIZE - 1);
            if(aln_adj != 0)
                new_curr_addr += (PAGESIZE - aln_adj);
            res = new_curr_addr;
            next = new_curr_addr + SB_REGION_EXPAND_SIZE;
            if (avail_sb.load().get_ptr() != nullptr){
                // ensure this expansion is necessary
                continue;
            }
            if (next > _rgs->regions[SB_IDX]->base_addr + _rgs->regions[SB_IDX]->FILESIZE){
                printf("\n----Region Manager: out of space in mmaped file-----\nCurr:%p\nBase:%p\n",res,_rgs->regions[SB_IDX]->base_addr);
                assert(0);
            }
            new_curr_addr = next;
            FLUSH(_rgs->regions[SB_IDX]->curr_addr_ptr);
            FLUSHFENCE;
            if(_rgs->regions[SB_IDX]->curr_addr_ptr->compare_exchange_strong(old_curr_addr, new_curr_addr)){
                FLUSH(_rgs->regions[SB_IDX]->curr_addr_ptr);
                FLUSHFENCE;
                DBG_PRINT("expand sb space for small sb allocation\n");
                organize_sb_list((char*)((uint64_t)res+SBSIZE), SB_REGION_EXPAND_SIZE/SBSIZE-1);
                Descriptor* desc = desc_lookup(res);
                new (desc) Descriptor();
                return (void*)res;
            }
            // CAS fails. Try to get a sb from free list again.
        }
    }
}
inline void BaseMeta::small_sb_retire(void* sb, size_t size){
    assert(size == SBSIZE);
    Descriptor* desc = desc_lookup(sb);
    new (desc) Descriptor(); // at this time we erase data in this desc
    ptr_cnt<Descriptor> oldhead = avail_sb.load();
    ptr_cnt<Descriptor> newhead;
    do{
        desc->next_free.store(oldhead.get_ptr());
        newhead.set(desc, oldhead.get_counter()+1);
    } while (!avail_sb.compare_exchange_weak(oldhead,newhead));
}

/* 
 * IMPORTANT: 	Large_sb_alloc is designed for very rare 
 *				large sb (>=16K) allocations. 
 *
 *				Every time sb region will be expanded by $size$
 */
inline void* BaseMeta::large_sb_alloc(size_t size){
    // cout<<"WARNING: Allocating a large object.\n";
    return expand_get_large_sb(size);
}

void BaseMeta::large_sb_retire(void* sb, size_t size){
    // cout<<"WARNING: Deallocating a large object.\n";
    assert(size%SBSIZE == 0);//size must be a multiple of SBSIZE
    organize_sb_list(sb, size/SBSIZE);
}

inline void* BaseMeta::alloc_large_block(size_t sz){
    return large_sb_alloc(sz);
}

void* BaseMeta::do_malloc(size_t size){
    if (UNLIKELY(size > MAX_SZ)) {
        // large block allocation
        size_t sbs = round_up(size, SBSIZE);//round size up to multiple of SBSIZE
        char* ptr = (char*)alloc_large_block(sbs);
        assert(ptr);
        Descriptor* desc = desc_lookup(ptr);

        desc->heap = &heaps[0];
        desc->block_size = sbs;
        desc->maxcount = 1;
        desc->superblock = ptr;

        Anchor anchor;
        anchor.avail = 0;
        anchor.count = 0;
        anchor.state = SB_FULL;
        desc->anchor.store(anchor);

        FLUSH(&desc);
        FLUSHFENCE;

        DBG_PRINT("large, ptr: %p", ptr);
        return (void*)ptr;
    }

    // size class calculation
    size_t sc_idx = get_sizeclass(size);

    TCacheBin* cache = &t_caches.t_cache[sc_idx];
    // fill cache if needed
    if (UNLIKELY(cache->get_block_num() == 0))
        fill_cache(sc_idx, cache);

    return cache->pop_block();
}
void BaseMeta::do_free(void* ptr){
    if(ptr==nullptr) return;
    assert(_rgs->in_range(SB_IDX,ptr));
    Descriptor* desc = desc_lookup(ptr);
    // @todo: this can happen with dynamic loading
    // need to print correct message

    size_t sc_idx = desc->heap->sc_idx;
    // DBG_PRINT("Desc %p, ptr %p", desc, ptr);

    // large allocation case
    if (UNLIKELY(!sc_idx)) {
        char* superblock = desc->superblock;
        // free superblock
        large_sb_retire(superblock, desc->block_size);
        return;
    }

    TCacheBin* cache = &t_caches.t_cache[sc_idx];
    SizeClassData* sc = get_sizeclass_by_idx(sc_idx);

    // flush cache if need
    if (UNLIKELY(cache->get_block_num() >= sc->cache_block_num))
        flush_cache(sc_idx, cache);

    cache->push_block((char*)ptr);
}


// this can be called by TCaches
void ralloc::public_flush_cache(){
    if(initialized) {
        for(int i=1;i<MAX_SZ_IDX;i++){// sc 0 is reserved.
            base_md->flush_cache(i, &t_caches.t_cache[i]);
        }
    }
}

/*
 * function GarbageCollection::operator()
 * 
 * Description:
 *  Sequential stop-the-world garbage collection routine for Ralloc when dirty
 *  segment exists.
 */
void GarbageCollection::operator() () {
    printf("Start garbage collection...\n");
    auto start = high_resolution_clock::now(); 
    // Step 0: initialize all transient data
    printf("Initializing all transient data...");
    base_md->avail_sb.off.store(nullptr); // initialize avail_sb
    for(int i = 0; i< MAX_SZ_IDX; i++) {
        // initialize partial list of each heap
        base_md->heaps[i].partial_list.off.store(nullptr);
    }
    printf("Initialized!\n");

    // Step 1: mark all accessible blocks from roots
    printf("Marking reachable nodes...");

    // First mark all root nodes
    for(int i = 0; i < MAX_ROOTS; i++) {
        if(base_md->roots[i]!=nullptr) {
            ralloc::roots_filter_func[i](base_md->roots[i], *this);
        }
    }
    while(!to_filter_node.empty()) {
        // pop nodes from the stack and call filter function of each node
        auto func = to_filter_func.top();
        auto node = to_filter_node.top();
        to_filter_node.pop();
        to_filter_func.pop();
        func(node,*this);
    }
    printf("Done!\nReachable blocks = %lu\n", marked_blk.size());

    // Step 2: sweep phase, update variables.
    printf("Reconstructing metadata...");
    char* curr_sb = _rgs->translate(SB_IDX, reinterpret_cast<char*>(SBSIZE)); // starting from first sb
    Descriptor* curr_desc = base_md->desc_lookup(curr_sb);
    auto curr_marked_blk = marked_blk.begin();
    char* sb_end = _rgs->regions[SB_IDX]->curr_addr_ptr->load();
    Descriptor* avail_sb = nullptr; // head of new free sb list

    // go through all sb in the region
    while(curr_sb < sb_end) {
        Anchor anchor(0, 0, SB_EMPTY);
        char* free_blocks_head = nullptr;
        char* last_possible_free_block = curr_sb;

        // go through all curr_marked_blk that's in this sb
        while (curr_marked_blk!=marked_blk.end() && 
                ((uint64_t)curr_sb>>SB_SHIFT) == ((uint64_t)(*curr_marked_blk)>>SB_SHIFT)) 
        { 
            // curr_marked_blk doesn't reach the end of marked_blk and curr_marked_blk is in curr_sb

            if(curr_desc->heap != nullptr &&
                curr_desc->superblock == curr_sb) {
                // false positive shouldn't enter here
                if(curr_desc->heap->sc_idx == 0) {
                    // large sb that's in use
                    // assert((*curr_marked_blk) == curr_sb); //allow large sb
                    // to be pointed at in the middle
                    assert(curr_desc->maxcount == 1);
                    anchor.state = SB_FULL; // set it as full
                } 
                else {
                    // small sb that's in use
                    anchor.state = SB_PARTIAL;
                    for(char* free_block = last_possible_free_block; 
                        free_block < (*curr_marked_blk); free_block+=curr_desc->block_size){
                        // put last_possible_free_block...(curr_marked_blk-1) to free blk list
                        (*reinterpret_cast<pptr<char>*>(free_block)) = free_blocks_head;
                        free_blocks_head = free_block;
                        anchor.count++;
                    }
                    last_possible_free_block = (*curr_marked_blk)+curr_desc->block_size;
                }
            }
            curr_marked_blk++;
        }
        if(anchor.state == SB_EMPTY) {
            // curr_sb isn't in use
            new (curr_desc) Descriptor();
            curr_desc->next_free.store(avail_sb);
            avail_sb = curr_desc;
            curr_sb+=SBSIZE;
            curr_desc++;
        } else {
            if(curr_desc->heap->sc_idx == 0) {
                // large sb that's in use

                anchor.avail = 0;
                anchor.count = 0;
                anchor.state = SB_FULL;

                // set transient variables in curr_desc
                curr_desc->next_free.store(nullptr);
                curr_desc->next_partial.store(nullptr);
                curr_desc->anchor.store(anchor);

                // move curr_sb to the sb next to this large sb
                curr_sb+=curr_desc->block_size;
                curr_desc = base_md->desc_lookup(curr_sb);
            } else {
                // small sb that's in use
                for(char* free_block = last_possible_free_block; 
                    free_block < curr_sb+curr_desc->maxcount*curr_desc->block_size; free_block+=curr_desc->block_size){
                    // put last_possible_free_block...(curr_sb+SBSIZE-1) to free blk list
                    (*reinterpret_cast<pptr<char>*>(free_block)) = free_blocks_head;
                    free_blocks_head = free_block;
                    anchor.count++;
                }
                if(anchor.count == 0) { 
                    // this sb is fully used
                    anchor.avail = curr_desc->maxcount;
                    anchor.state = SB_FULL;

                    // set transient variables in curr_desc
                    curr_desc->next_free.store(nullptr);
                    curr_desc->next_partial.store(nullptr);
                    curr_desc->anchor.store(anchor);
                } else {
                    // this sb is partially used
                    assert(free_blocks_head != nullptr);
                    assert((uint64_t)(free_blocks_head - curr_sb)%curr_desc->block_size == 0);
                    anchor.avail = (uint64_t)(free_blocks_head - curr_sb)/curr_desc->block_size;
                    anchor.state = SB_PARTIAL; // it must be SB_PARTIAL already but we assign it anyway

                    // set transient variables in curr_desc
                    curr_desc->next_free.store(nullptr);
                    base_md->heap_push_partial(curr_desc);
                    curr_desc->anchor.store(anchor);
                }
                // move curr_sb and curr_desc to next sb
                curr_sb+=SBSIZE;
                curr_desc++;
            }
        }
    }
    // store head of new free sb list into base_md
    ptr_cnt<Descriptor> tmp_avail_sb(avail_sb, 0);
    base_md->avail_sb.store(tmp_avail_sb);
    printf("Reconstructed! \n");
    auto stop = high_resolution_clock::now(); 
    assert(curr_marked_blk == marked_blk.end());
    auto duration = duration_cast<milliseconds>(stop - start);
    cout << "Time elapsed = " << duration.count() <<" ms on GC."<<endl;


    printf("Flushing recovered data...");
    _rgs->flush_region(DESC_IDX);
    _rgs->flush_region(SB_IDX);
    char* addr_to_flush = reinterpret_cast<char*>(base_md);
    // flush values in BaseMeta, including avail_sb and partial lists
    for(size_t i = 0; i < sizeof(BaseMeta); i += CACHELINE_SIZE) {
        addr_to_flush += CACHELINE_SIZE;
        FLUSH(addr_to_flush);
    }
    FLUSHFENCE;
    printf("Garbage collection Completed!\n");
}
