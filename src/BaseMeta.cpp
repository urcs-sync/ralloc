/*
 * Copyright (C) 2007  Scott Schneider, Christos Antonopoulos
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <sys/mman.h>

#include <string>

#include "BaseMeta.hpp"

// some metadata from LRMalloc
__thread TCacheBin BaseMeta::t_cache[MAX_SZ_IDX];

using namespace std;

BaseMeta::BaseMeta(RegionManager* m, uint64_t thd_num) : 
	mgr(m),
	thread_num(thd_num) {
	FLUSH(&thread_num);
	// free_desc = new ArrayQueue<Descriptor*>("pmmalloc_freedesc");
	free_sb = new ArrayStack<void*>("pmmalloc_freesb");
	/* allocate these persistent data into specific memory address */

	/* heaps init */
	for (size_t idx = 0; idx < MAX_SZ_IDX; ++idx){
		ProcHeap& heap = heaps[idx];
		heap.partial_list.store({ nullptr });
		heap.sc_idx = idx;
		FLUSH(&heaps[idx]);
	}

	/* persistent roots init */
	for(int i=0;i<MAX_ROOTS;i++){
		roots[i]=nullptr;
		FLUSH(&roots[i]);
	}

	FLUSHFENCE;
}

uint64_t BaseMeta::new_space(int i){//i=0:desc, i=1:small sb, i=2:large sb
	FLUSHFENCE;
	uint64_t my_space_num = space_num[i].fetch_add(1,std::memory_order_relaxed);
	if(my_space_num>=MAX_SECTION) assert(0&&"space number reaches max!");
	FLUSH(&space_num[i]);
	spaces[i][my_space_num].sec_bytes = 0;
	FLUSH(&spaces[i][my_space_num].sec_bytes);
	FLUSHFENCE;
	uint64_t space_size = i==0?DESC_SPACE_SIZE:SB_SPACE_SIZE;
	bool res = mgr->__nvm_region_allocator(&(spaces[i][my_space_num].sec_start),PAGESIZE, space_size);
	if(!res) assert(0&&"region allocation fails!");
	// spaces[i][my_space_num].sec_curr.store(spaces[i][my_space_num].sec_start);
	spaces[i][my_space_num].sec_bytes = space_size;
	FLUSH(&spaces[i][my_space_num].sec_start);
	// FLUSH(&spaces[i][my_space_num].sec_curr);
	FLUSH(&spaces[i][my_space_num].sec_bytes);
	FLUSHFENCE;
	return my_space_num;
}

inline size_t BaseMeta::get_sizeclass(size_t size){
	return sizeclass.get_sizeclass(size);
}

inline SizeClassData* BaseMeta::get_sizeclass(ProcHeap* h){
	return get_sizeclass_by_idx(h->sc_idx);
}

inline SizeClassData* BaseMeta::get_sizeclass_by_idx(size_t idx) { 
	return &sizeclass.get_sizeclass_by_idx(idx);
}

// compute block index in superblock by addr to sb, block, and sc index
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
	assert(block_num > 0);
	assert(block_num <= sc->cache_block_num);
}

//todo after this point: persist
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
		PageInfo info = get_page_info_for_ptr(head);
		Descriptor* desc = info.get_desc();
		char* superblock = desc->superblock;

		// cache is a linked list of blocks
		// superblock free list is also a linked list of blocks
		// can optimize transfers of blocks between these 2 entities
		//  by exploiting existing structure
		uint32_t block_count = 1;
		// check if next cache blocks are in the same superblock
		// same superblock, same descriptor
		while (cache->get_block_num() > block_count) {
			char* ptr = *(char**)tail;
			if (ptr < superblock || ptr >= superblock + sb_size)
				break; // ptr not in superblock

			// ptr in superblock, add to "list"
			++block_count;
			tail = ptr;
		}

		cache->pop_list(*(char**)tail, block_count);

		// add list to desc, update anchor
		uint32_t idx = compute_idx(superblock, head, sc_idx);

		Anchor oldanchor = desc->anchor.load();
		Anchor newanchor;
		do {
			// update anchor.avail
			char* next = (char*)(superblock + oldanchor.avail * block_size);
			*(char**)tail = next;

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

		// CAS success, can free block
		if (newanchor.state == SB_EMPTY) {
			// unregister descriptor
			unregister_desc(heap, superblock);

			// free superblock
			page_free(superblock, heap->get_sizeclass()->sb_size);
		}
		else if (oldanchor.state == SB_FULL)
			heap_push_partial(desc);
	}
}

// (un)register descriptor pages with pagemap
// all pages used by the descriptor will point to desc in
//  the pagemap
// for (unaligned) large allocations, only first page points to desc
// aligned large allocations get the corresponding page pointing to desc
void BaseMeta::update_pagemap(ProcHeap* heap, char* ptr, Descriptor* desc, size_t sc_idx) {
	assert(ptr);

	PageInfo info;
	info.set(desc, sc_idx);

	// large allocation, don't need to (un)register every page
	// just first
	if (!heap)
	{
		pagemap.set_page_info(ptr, info);
		return;
	}

	// only need to worry about alignment for large allocations
	// assert(ptr == superblock);

	// small allocation, (un)register every page
	// could *technically* optimize if block_size >>> page, 
	//  but let's not worry about that
	size_t sb_size = get_sizeclass(heap)->sb_size;
	// sb_size is a multiple of page
	assert((sb_size & PAGE_MASK) == 0);
	for (size_t idx = 0; idx < sb_size; idx += PAGESIZE)
		pagemap.set_page_info(ptr + idx, info); 
}

void BaseMeta::register_desc(Descriptor* desc)
{
	ProcHeap* heap = desc->heap;
	char* ptr = desc->superblock;
	size_t sc_idx = 0;
	if (LIKELY(heap != nullptr))
		sc_idx = heap->sc_idx;

	update_pagemap(heap, ptr, desc, sc_idx);
}

// unregister descriptor before superblock deletion
// can only be done when superblock is about to be free'd to OS
inline void BaseMeta::unregister_desc(ProcHeap* heap, char* superblock)
{
	update_pagemap(heap, superblock, nullptr, 0L);
}

inline PageInfo BaseMeta::get_page_info_for_ptr(void* ptr)
{
	return paegmap.get_page_info((char*)ptr);
}

//todo after this point: understand func impl.
void BaseMeta::heap_push_partial(Descriptor* desc)
{
	ProcHeap* heap = desc->heap;
	std::atomic<DescriptorNode>& list = heap->partial_list;

	DescriptorNode oldhead = list.load();
	DescriptorNode newhead;
	newhead.set(desc, oldhead.get_counter() + 1);
	do
	{
		assert(oldhead.get_desc() != newhead.get_desc());
		newhead.get_desc()->next_partial.store(oldhead); 
	}
	while (!list.compare_exchange_weak(oldhead, newhead));
}

Descriptor* BaseMeta::heap_pop_partial(ProcHeap* heap)
{
	std::atomic<DescriptorNode>& list = heap->partial_list;
	DescriptorNode oldhead = list.load();
	DescriptorNode newhead;
	do
	{
		Descriptor* olddesc = oldhead.get_desc();
		if (!olddesc)
			return nullptr;

		newhead = olddesc->next_partial.load();
		Descriptor* desc = newhead.get_desc();
		uint64_t counter = oldhead.get_counter();
		newhead.set(desc, counter);
	}
	while (!list.compare_exchange_weak(oldhead, newhead));

	return oldhead.get_desc();
}

void malloc_from_partial(size_t sc_idx, TCacheBin* cache, size_t& block_num)
{
	ProcHeap* heap = &heaps[sc_idx];

retry:
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
	do
	{
		if (oldanchor.state == SB_EMPTY)
		{
			desc_retire(desc);
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

void malloc_from_newsb(size_t sc_idx, TCacheBin* cache, size_t& block_num)
{
	ProcHeap* heap = &heaps[sc_idx];
	SizeClassData* sc = get_sizeclass_by_idx(sc_idx);

	Descriptor* desc = desc_alloc();
	assert(desc);

	uint32_t const block_size = sc->block_size;
	uint32_t const maxcount = sc->get_block_num();

	desc->heap = heap;
	desc->block_size = block_size;
	desc->maxcount = maxcount;
	desc->superblock = (char*)page_alloc(sc->sb_size);

	// prepare block list
	char* superblock = desc->superblock;
	for (uint32_t idx = 0; idx < maxcount - 1; ++idx)
	{
		char* block = superblock + idx * block_size;
		char* next = superblock + (idx + 1) * block_size;
		*(char**)block = next;
	}

	// push blocks to cache
	char* block = superblock; // first block
	cache->push_list(block, maxcount);

	Anchor anchor;
	anchor.avail = maxcount;
	anchor.count = 0;
	anchor.state = SB_FULL;

	desc->anchor.store(anchor);

	assert(anchor.avail < maxcount || anchor.state == SB_FULL);
	assert(anchor.count < maxcount);

	// register new descriptor
	// must be done before setting superblock as active
	// or leaving superblock as available in a partial list
	register_desc(desc);

	// if state changes to SB_PARTIAL, desc must be added to partial list
	assert(anchor.state == SB_FULL);

	block_num += maxcount;
}









void* BaseMeta::small_sb_alloc(){
	void* sb = nullptr;
	if(auto tmp = free_sb->pop()){
		sb = tmp.value();
	}
	else{
		uint64_t space_num = new_space(1);
		cout<<"allocate sb space "<<space_num<<endl;
		sb = spaces[1][space_num].sec_start;
		organize_sb_list(sb,SB_SPACE_SIZE/SBSIZE,SBSIZE);
	}
	return sb;
}
void BaseMeta::small_sb_retire(void* sb){
	free_sb->push(sb);
}
//todo
void* BaseMeta::large_sb_alloc(size_t size, uint64_t alignement){
	cout<<"WARNING: Allocating a large object is not persisted yet!\n";
	// assert(0&"not persistently implemented yet!");
	void* addr = mmap(nullptr,size,PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (addr == MAP_FAILED) {
		fprintf(stderr, "large_sb_alloc() mmap failed, %lu: ", size);
		switch (errno) {
			case EBADF:	fprintf(stderr, "EBADF"); break;
			case EACCES:	fprintf(stderr, "EACCES"); break;
			case EINVAL:	fprintf(stderr, "EINVAL"); break;
			case ETXTBSY:	fprintf(stderr, "ETXBSY"); break;
			case EAGAIN:	fprintf(stderr, "EAGAIN"); break;
			case ENOMEM:	fprintf(stderr, "ENOMEM"); break;
			case ENODEV:	fprintf(stderr, "ENODEV"); break;
		}
		fprintf(stderr, "\n");
		fflush(stderr);
		exit(1);
	}
	else if(addr == nullptr){
		fprintf(stderr, "large_sb_alloc() mmap of size %lu returned NULL\n", size);
		fflush(stderr);
		exit(1);
	}
	return addr;
}
//todo
void BaseMeta::large_sb_retire(void* sb, size_t size){
	// assert(0&"not persistently implemented yet!");
	munmap(sb, size);
}

void* BaseMeta::malloc_from_partial(Procheap* heap){
	Descriptor* desc = nullptr;
	Anchor oldanchor,newanchor;
	uint64_t morecredits = 0;
	void* addr;

retry:
	//grab the partial desc from heap or sizeclass (exclusively)
	desc = heap_get_partial(heap);
	if(!desc){
		return nullptr;
	}
	desc->heap = heap;
	FLUSH(&desc->heap);
	oldanchor = desc->anchor.load(std::memory_order_acquire);
	TFLUSH(&desc->anchor);
	do{
		//reserve blocks
		newanchor = oldanchor;
		if(oldanchor.state == EMPTY){
			desc_retire(desc);
			goto retry;
		}
		//oldanchor state must be PARTIAL, and count must > 0
		morecredits = min(oldanchor.count - 1, MAXCREDITS);
		newanchor.count -= morecredits + 1;
		newanchor.state = (morecredits>0)?ACTIVE:FULL;
		TFLUSHFENCE;
	}while(!desc->anchor.compare_exchange_strong(oldanchor,newanchor,std::memory_order_acq_rel));
	TFLUSH(&desc->anchor);
	TFLUSHFENCE;

	oldanchor = desc->anchor.load(std::memory_order_acquire);
	TFLUSH(&desc->anchor);
	do{
		//pop reserved block
		newanchor = oldanchor;
		addr = (void*)((uint64_t)desc->sb + oldanchor.avail * desc->sz);
		newanchor.avail = *(uint64_t*)addr;
		newanchor.tag++;
		TFLUSHFENCE;
	}while(!desc->anchor.compare_exchange_strong(oldanchor,newanchor,std::memory_order_acq_rel));
	TFLUSH(&desc->anchor);
	TFLUSHFENCE;

	if(morecredits > 0){
		update_active(heap, desc, morecredits);
	}
	*((char*)addr) = (char)SMALL;
	addr += TYPE_SIZE;
	*((Descriptor**)addr) = desc;
	FLUSH(addr-TYPE_SIZE);
	FLUSH(addr);
	FLUSHFENCE;
	return ((void *)((uint64_t)addr + PTR_SIZE));

}
void* BaseMeta::malloc_from_newsb(Procheap* heap){
	Active oldactive,newactive;
	Anchor newanchor;
	void* addr = nullptr;

	Descriptor* desc = desc_alloc();
	desc->sb = small_sb_alloc();
	desc->heap = heap;
	newanchor.avail = 1;
	desc->sz = heap->sc->sz;
	desc->maxcount = heap->sc->sbsize / desc->sz;
	FLUSH(&desc->sb);
	FLUSH(&desc->heap);
	FLUSH(&desc->sz);
	FLUSH(&desc->maxcount);
	organize_blk_list(desc->sb, desc->maxcount, desc->sz);

	newactive.ptr = (uint64_t)desc>>6;
	newactive.credits = min(desc->maxcount - 1, MAXCREDITS)-1;
	newanchor.count = max(((int)desc->maxcount - 1)-((int)newactive.credits + 1), 0);
	newanchor.state = ACTIVE;
	TFLUSHFENCE;
	desc->anchor.store(newanchor,std::memory_order_release);
	TFLUSH(&desc->anchor);

	*((uint64_t*)(&oldactive)) = 0;

	TFLUSHFENCE;
	if(heap->active.compare_exchange_strong(oldactive,newactive,std::memory_order_acq_rel)){
		TFLUSH(&heap->active);
		addr = desc->sb;
		*((char*)addr) = (char)SMALL;
		addr += TYPE_SIZE;
		*((Descriptor**)addr) = desc;
		FLUSH(addr-TYPE_SIZE);
		FLUSH(addr);
		FLUSHFENCE;
		return (void*)((uint64_t)addr + PTR_SIZE);
	}
	else {
		small_sb_retire(desc->sb);
		desc_retire(desc);
		return nullptr;
	}
}
void* BaseMeta::alloc_large_block(size_t sz){
	void* addr = large_sb_alloc(sz + HEADER_SIZE, SBSIZE);
	*((char*)addr) = (char)LARGE;
	addr += TYPE_SIZE;
	*((uint64_t*)addr) = sz + HEADER_SIZE;
	FLUSH(addr-TYPE_SIZE);
	FLUSH(addr);
	FLUSHFENCE;
	return (void*)(addr + PTR_SIZE);
}