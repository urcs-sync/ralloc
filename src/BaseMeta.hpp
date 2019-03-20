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

struct Descriptor;

/* data structures */
struct Sizeclass{
	PM_TRANSIENT ArrayQueue<Descriptor*,PARTIAL_CAP>* partial_desc;
	PM_PERSIST unsigned int sz; // block size in byte
	PM_PERSIST unsigned int sbsize; // superblock size in byte
	Sizeclass(unsigned int sbs = SBSIZE);
	~Sizeclass(){delete partial_desc;}
	void init(unsigned int bs);
	void cleanup(){partial_desc->cleanup();}
}__attribute__((aligned(CACHE_LINE_SIZE)));

struct Active{
	uint64_t ptr:58, credits:6;
	Active(){ptr=0;credits=0;}
	Active(uint64_t in){ptr=in>>6;credits=in&0x3f;}
};

struct Procheap {
	PM_TRANSIENT std::atomic<Active> active;			// initially NULL
	PM_TRANSIENT std::atomic<Descriptor*> partial;	// initially NULL, pointer to the partially used sb's desc
	PM_PERSIST Sizeclass* sc;					// pointer to parent sizeclass
	Procheap(Sizeclass* s = nullptr,
			uint64_t a = 0,
			Descriptor* p = nullptr):
		active(a),
		partial(p),
		sc(s) {
			FLUSH(&active);
			FLUSH(&partial);
			FLUSH(&sc);
			FLUSHFENCE;
		};
}__attribute__((aligned(CACHE_LINE_SIZE)));

struct Anchor{
	uint64_t avail:24,count:24, state:2, tag:14;
	Anchor(uint64_t a = 0){(*(uint64_t*)this) = a;}
	Anchor(unsigned a, unsigned c, unsigned s, unsigned t):
		avail(a),count(c),state(s),tag(t){};
};

struct Descriptor{
	PM_TRANSIENT std::atomic<Anchor> anchor;
	PM_PERSIST void* sb;				// pointer to superblock
	PM_PERSIST Procheap* heap;			// pointer to owner procheap
	PM_PERSIST unsigned int sz;			// block size
	PM_PERSIST unsigned int maxcount;	// superblock size / sz
}__attribute__ ((aligned (64))); //align to 64 so that last 6 of active can use for credits

struct Section {
	PM_PERSIST void* sec_start;
	// PM_PERSIST std::atomic<void*> sec_curr;
	PM_PERSIST size_t sec_bytes;
}__attribute__((aligned(CACHE_LINE_SIZE)));

class BaseMeta{
	/* transient metadata and tools */
	PM_TRANSIENT RegionManager* mgr;//assigned when BaseMeta constructs
	PM_TRANSIENT ArrayQueue<Descriptor*>* free_desc;
	//todo: make free_sb FILO to mitigate page fault
	PM_TRANSIENT ArrayStack<void*>* free_sb;//unused small sb

	/* persistent metadata defined here */
	//base metadata
	PM_PERSIST uint64_t thread_num;
	//TODO: other metadata

	PM_PERSIST void* roots[MAX_ROOTS];//persistent root
	PM_PERSIST Sizeclass sizeclasses[MAX_SMALLSIZE/GRANULARITY];
	PM_PERSIST Procheap procheaps[PROCHEAP_NUM][MAX_SMALLSIZE/GRANULARITY];

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
	inline uint64_t min(uint64_t a, uint64_t b){return a>b?b:a;}
	inline uint64_t max(uint64_t a, uint64_t b){return a>b?a:b;}
	inline void set_mgr(RegionManager* m){mgr = m;}
	uint64_t new_space(int i);//i=0:desc, i=1:small sb, i=2:large sb. return index of allocated space.
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
		free_desc = new ArrayQueue<Descriptor*>("pmmalloc_freedesc");
		free_sb = new ArrayStack<void*>("pmmalloc_freesb");
		for(int i=0;i<MAX_SMALLSIZE/GRANULARITY;i++){
			sizeclasses[i].partial_desc = 
				new ArrayQueue<Descriptor*,PARTIAL_CAP>("scpartial"+std::to_string((i+1)*GRANULARITY));
		}
	}
	void cleanup(){
		//flush everything before exit
		delete free_desc;
		delete free_sb;
		for(int i=0;i<MAX_SMALLSIZE/GRANULARITY;i++){
			sizeclasses[i].cleanup();
		}
	}

	void* small_sb_alloc();
	void small_sb_retire(void* sb);
	void* large_sb_alloc(size_t size, uint64_t alignement);
	void large_sb_retire(void* sb, size_t size);
	void organize_desc_list(Descriptor* start, uint64_t count, uint64_t stride);// put new descs to free_desc queue
	void organize_sb_list(void* start, uint64_t count, uint64_t stride);// put new sbs to free_sb queue
	void organize_blk_list(void* start, uint64_t count, uint64_t stride);//create linked freelist of blocks in the sb
	
	Descriptor* desc_alloc();
	void desc_retire(Descriptor* desc);
	Descriptor* list_get_partial(Sizeclass* sc);//get a partial desc
	void list_put_partial(Descriptor* desc);//put a partial desc to partial_desc
	Descriptor* heap_get_partial(Procheap* heap);
	void heap_put_partial(Descriptor* desc);
	void list_remove_empty_desc(Sizeclass* sc);//try to retire an empty desc from sc->partial_desc
	void remove_empty_desc(Procheap* heap, Descriptor* desc);//remove the empty desc from heap->partial, or run list_remove_empty_desc on heap->sc
	void update_active(Procheap* heap, Descriptor* desc, uint64_t morecredits);
	Descriptor* mask_credits(Active oldactive);

	Procheap* find_heap(size_t sz);
	void* malloc_from_active(Procheap* heap);
	void* malloc_from_partial(Procheap* heap);
	void* malloc_from_newsb(Procheap* heap);
	void* alloc_large_block(size_t sz);
};


#endif /* _BASE_META_HPP_ */