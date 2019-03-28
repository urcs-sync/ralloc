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

#ifndef _BASE_META_HPP_
#define _BASE_META_HPP_

#include <atomic>
#include <iostream>

#include "pm_config.hpp"
#include "thread_util.hpp"

#include "RegionManager.hpp"
#include "ArrayStack.hpp"
#include "ArrayQueue.hpp"
#include "SizeClass.hpp"
#include "TCache.hpp"
#include "PageMap.hpp"

/********class BaseMeta********
 * This is the file where meta data structures of
 * pmmalloc are defined.
 *
 * The logic is: 
 * 	1. 	Use RegionManager to allocate a persistent 
 * 		region with size of BaseMeta
 * 	2. 	Use the pointer to the persistent region 
 * 		as BaseMeta*
 * 	3. 	Initialize BaseMeta* by calling constructor 
 * 		in place.
 *
 * Some useful functions to know:
 * 		void BaseMeta::restart():
 * 			Restart the BaseMeta after data is remapped in
 *			BaseMeta. It traces and recovers free lists.
 *		void BaseMeta::cleanup():
 *			Do cleanup before program exits.
 *			It flushes free lists and marks as clean.
 *		void set_mgr(RegionManager* m):
 *			Set mgr pointer to m.
 *			It should be called after data is remapped in
 *			BaseMeta.
 *		void* set_root(void* ptr, uint64_t i):
 *			Set persistent root i to ptr, return the old root
 *			in root i if there is, or nullptr if there isn't.
 *		void* get_root(uint64_t i):
 *			Return persistent root i, or nullptr if there isn't.
 *
 * Procedure to do a fresh start mapping to $filepath$:
 * 		BaseMeta* base_md;
 * 		RegionManager* mgr = new RegionManager(filepath);
 * 		bool res = mgr->__nvm_region_allocator((void**)&base_md,sizeof(void*),sizeof(BaseMeta));
 * 		if(!res) assert(0&&"mgr allocation fails!");
 * 		mgr->__store_heap_start(base_md);
 * 		new (base_md) BaseMeta(mgr, thd_num);
 *
 * Procedure to restart from $filepath$:
 * 		BaseMeta* base_md;
 * 		RegionManager* mgr = new RegionManager(filepath);
 * 		void* hstart = mgr->__fetch_heap_start();
 * 		base_md = (BaseMeta*) hstart;
 * 		base_md->set_mgr(mgr);
 * 		base_md->restart();
 *
 * By default free_sb is mapped to $(HEAPFILE_PREFIX)_stack_pmmalloc_freesb,
 * free_desc to $(HEAPFILE_PREFIX)_queue_pmmalloc_freedesc, 
 * and partial_desc of each sizeclass with block size sz to 
 * $(HEAPFILE_PREFIX)_queue_scpartial$(sz).
 *
 * Most of functions related to malloc and free share large portion of 
 * code with the open source project https://github.com/scotts/michael.
 * Some modifications were applied for bug fixing or functionality adjustment.
 *
 * Adapted and reimplemented by:
 * 		Wentao Cai (wcai6@cs.rochester.edu)
 */

// superblock states
// used in Anchor::state
enum SuperblockState
{
	// all blocks allocated or reserved
	SB_FULL		= 0,
	// has unreserved available blocks
	SB_PARTIAL	= 1,
	// all blocks are free
	SB_EMPTY	= 2,
};

struct Anchor;
struct DescriptorNode;
struct Descriptor;
struct ProcHeap;

/* data structures */
struct Anchor{
	uint64_t avail:31,count:31, state:2;
	Anchor(uint64_t a = 0){(*(uint64_t*)this) = a;}
	Anchor(unsigned a, unsigned c, unsigned s):
		avail(a),count(c),state(s){};
};
static_assert(sizeof(Anchor) == sizeof(uint64_t), "Invalid anchor size");

struct DescriptorNode
{
public:
	// ptr
	Descriptor* _desc = nullptr;
	// aba counter

public:
	void set(Descriptor* desc, uint64_t counter)
	{
		// desc must be cacheline aligned
		assert(((uint64_t)desc & CACHELINE_MASK) == 0);
		// counter may be incremented but will always be stored in
		//  LG_CACHELINE bits
		_desc = (Descriptor*)((uint64_t)desc | (counter & CACHELINE_MASK));
	}

	Descriptor* get_desc() const
	{
		return (Descriptor*)((uint64_t)_desc & ~CACHELINE_MASK);
	}

	uint64_t get_counter() const
	{
		return (uint64_t)((uint64_t)_desc & CACHELINE_MASK);
	}

};

static_assert(sizeof(DescriptorNode) == sizeof(uint64_t), "Invalid descriptor node size");

/* Superblock descriptor
 * needs to be cache-line aligned
 * descriptors are allocated and *never* freed
 * 
 * During recovery desc space will be scanned and
 * all desc whose in_use is true will be added
 * to pagemap again.
 */
struct Descriptor
{
	// list node pointers
	// used in free descriptor list
	std::atomic<DescriptorNode> next_free;
	// used in partial descriptor list
	std::atomic<DescriptorNode> next_partial;
	// anchor
	std::atomic<Anchor> anchor;

	char* superblock;
	ProcHeap* heap;
	uint32_t block_size; // block size
	uint32_t maxcount;
	bool in_use; // false if it's free, true if it's in use
}__attribute__((aligned(CACHELINE_SIZE)));

// at least one ProcHeap instance exists for each sizeclass
struct ProcHeap
{
public:
	// ptr to descriptor, head of partial descriptor list
	std::atomic<DescriptorNode> partial_list;
	// size class index
	size_t sc_idx;
}__attribute__((aligned(CACHELINE_SIZE)));

//persistent sections
struct Section {
	PM_PERSIST void* sec_start;
	// PM_PERSIST std::atomic<void*> sec_curr;
	PM_PERSIST size_t sec_bytes;
}__attribute__((aligned(CACHELINE_SIZE)));

class BaseMeta{
	/* transient metadata and tools */
	PM_TRANSIENT RegionManager* mgr;//assigned when BaseMeta constructs
	//unused small sb
	PM_TRANSIENT ArrayStack<void*>* free_sb;
	// descriptor recycle list
	PM_TRANSIENT std::atomic<DescriptorNode> avail_desc;
	// metadata for pagemap
	PM_TRANSIENT PageMap pagemap;
	/* sizeclass data for lookup. It's determined statically so we make it transient */
	PM_TRANSIENT SizeClass sizeclass;
	/* thread-local cache */
	PM_TRANSIENT static __thread TCacheBin t_cache[MAX_SZ_IDX]
		__attribute__((aligned(CACHELINE_SIZE)));

	/* persistent metadata defined here */
	//base metadata
	PM_PERSIST uint64_t thread_num;

	PM_PERSIST ProcHeap heaps[MAX_SZ_IDX];

	PM_PERSIST void* roots[MAX_ROOTS] = {nullptr};//persistent root
	PM_PERSIST std::atomic<uint64_t> space_num[3];//0:desc, 1:small sb, 2: large sb
	PM_PERSIST Section spaces[3][MAX_SECTION];//0:desc, 1:small sb, 2: large sb
	/* persistent metadata ends here */
public:
	BaseMeta(RegionManager* m, uint64_t thd_num = MAX_THREADS);
	~BaseMeta(){
		//usually BaseMeta shouldn't be destructed, and will be reused in the next time
		std::cout<<"Warning: BaseMeta is being destructed!\n";
		cleanup();
	}
	void* do_malloc(size_t size);
	void do_free(void* ptr);
	inline void set_mgr(RegionManager* m){mgr = m;}
	inline uint64_t min(uint64_t a, uint64_t b){return a>b?b:a;}
	inline uint64_t max(uint64_t a, uint64_t b){return a>b?a:b;}
	inline void* set_root(void* ptr, uint64_t i){
		//this is sequential
		assert(i<MAX_ROOTS);
		void* res = nullptr;
		if(roots[i]!=nullptr) res = roots[i];
		roots[i] = ptr;
		FLUSH(&roots[i]);
		FLUSHFENCE;
		return res;
	}
	inline void* get_root(uint64_t i){
		//this is sequential
		assert(i<MAX_ROOTS);
		return roots[i];
	}
	void restart(){
		// free_desc = new ArrayQueue<Descriptor*>("pmmalloc_freedesc");
		free_sb = new ArrayStack<void*>("pmmalloc_freesb");
	}
	void cleanup(){
		//flush everything before exit
		// delete free_desc;
		delete free_sb;
	}

private:
	//i=0:desc, i=1:small sb, i=2:large sb. return index of allocated space
	uint64_t new_space(int i);
	// get one free sb or allocate a new space for sbs
	void* small_sb_alloc(size_t size);
	// free the superblock sb points to
	void small_sb_retire(void* sb, size_t size);
	// add all newly allocated sbs to free_sb
	void organize_sb_list(void* start, uint64_t count, uint64_t stride);
	void* large_sb_alloc(size_t size, uint64_t alignement);
	void large_sb_retire(void* sb, size_t size);
	void* alloc_large_block(size_t sz);

	// func on size class
	size_t get_sizeclass(size_t size);
	SizeClassData* get_sizeclass(ProcHeap* h);
	SizeClassData* get_sizeclass_by_idx(size_t idx);
	uint32_t compute_idx(char* superblock, char* block, size_t sc_idx);

	// func on cache
	void fill_cache(size_t sc_idx, TCacheBin* cache);
	void flush_cache(size_t sc_idx, TCacheBin* cache);

	// func on page map
	void update_pagemap(ProcHeap* heap, char* ptr, Descriptor* desc, size_t sc_idx);
	void register_desc(Descriptor* desc);
	void unregister_desc(ProcHeap* heap, char* superblock);
	PageInfo get_page_info_for_ptr(void* ptr);

	// helper func
	void heap_push_partial(Descriptor* desc);
	Descriptor* heap_pop_partial(ProcHeap* heap);
	void malloc_from_partial(size_t sc_idx, TCacheBin* cache, size_t& block_num);
	void malloc_from_newsb(size_t sc_idx, TCacheBin* cache, size_t& block_num);
	Descriptor* desc_alloc();
	void desc_retire(Descriptor* desc);
};


#endif /* _BASE_META_HPP_ */