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
#include "pmmalloc.hpp"

#include <string>
#include <atomic>
#include <vector>
#include <algorithm>

using namespace std;

/* 
 * mmap the existing heap file corresponding to id. aka restart,
 * 		and if multiple heaps exist, print out and let user select;
 * if such a heap doesn't exist, create one. aka start.
 * id is the distinguishable identity of applications.
 */
pmmalloc::pmmalloc(std::string id, uint64_t thd_num) : 
	thread_num(thd_num){
	filepath = HEAPFILE_PREFIX + id;
	bool restart = RegionManager::exists_test(filepath);
	cout<<"sizeof basemeta:"<<sizeof(BaseMeta)<<endl;

	//TODO: find all heap files with this id to determine the value of restart, and assign appropriate path to filepath
	if(restart){
		mgr = new RegionManager(filepath);
		void* hstart = mgr->__fetch_heap_start();
		base_md = (BaseMeta*) hstart;
		base_md->set_mgr(mgr);
		base_md->restart();
		//collect if the heap is dirty
	} else {
		/* RegionManager init */
		mgr = new RegionManager(filepath);
		bool res = mgr->__nvm_region_allocator((void**)&base_md,sizeof(void*),sizeof(BaseMeta));
		if(!res) assert(0&&"mgr allocation fails!");
		mgr->__store_heap_start(base_md);
		new (base_md) BaseMeta(mgr, thd_num);
	}
}

//destructor aka close
pmmalloc::~pmmalloc(){
	//close
	base_md->cleanup();
	delete mgr;
}

//manually request to collect garbage
bool pmmalloc::collect(){
	//TODO
	return true;
}

void* pmmalloc::p_malloc(size_t sz, std::vector<void*>(*f)){
	//TODO: put f in each block
	Procheap *heap;
	void* addr;
	DBG_PRINT("malloc() sz %lu\n", sz);
	// Use sz and thread id to find heap.
	heap = base_md->find_heap(sz);

	if (!heap) {
		// Large block
		addr = base_md->alloc_large_block(sz);
		DBG_PRINT("Large block allocation: %p\n", addr);
		return addr;
	}

	while(1) { 
		addr = base_md->malloc_from_active(heap);
		if (addr) {
			DBG_PRINT("malloc() return MallocFromActive %p\n", addr);
			return addr;
		}
		addr = base_md->malloc_from_partial(heap);
		if (addr) {
			DBG_PRINT("malloc() return MallocFromPartial %p\n", addr);
			return addr;
		}
		addr = base_md->malloc_from_newsb(heap);
		if (addr) {
			DBG_PRINT("malloc() return MallocFromNewSB %p\n", addr);
			return addr;
		}
	} 
}

void pmmalloc::p_free(void* ptr){
	Descriptor* desc;
	void* sb;
	Anchor oldanchor, newanchor;
	Procheap* heap = NULL;
	DBG_PRINT("Calling my free %p\n", ptr);

	if (!ptr) {
		return;
	}
	
	// if(!mgr->__within_range(ptr)) {
	// //this block wasn't allocated by pmmalloc, call regular free by default.
	// 	free(ptr);
	// 	return;
	// }
	// get prefix
	ptr = (void*)((uint64_t)ptr - HEADER_SIZE);  
	if (*((char*)ptr) == (char)LARGE) {
		DBG_PRINT("Freeing large block\n");
		base_md->large_sb_retire(ptr, *((uint64_t *)(ptr + TYPE_SIZE)));
		return;
	}
	desc = *((Descriptor**)((uint64_t)ptr + TYPE_SIZE));
	
	sb = desc->sb;
	oldanchor = desc->anchor.load(std::memory_order_acquire);
	do { 
		newanchor = oldanchor;

		*((uint64_t*)ptr) = oldanchor.avail;
		newanchor.avail = ((uint64_t)ptr - (uint64_t)sb) / desc->sz;

		if (oldanchor.state == FULL) {
			DBG_PRINT("Marking superblock %p as PARTIAL\n", sb);
			newanchor.state = PARTIAL;
		}

		if (oldanchor.count == desc->maxcount - 1) {
			heap = desc->heap;
			INSTRFENCE;// instruction fence.
			DBG_PRINT("Marking superblock %p as EMPTY; count %d\n", sb, oldanchor.count);
			newanchor.state = EMPTY;
		} 
		else {
			++newanchor.count;
		}
		TFLUSHFENCE;
	} while (!desc->anchor.compare_exchange_strong(oldanchor, newanchor,std::memory_order_acq_rel));
	TFLUSH(&desc->anchor);
	TFLUSHFENCE;
	if (newanchor.state == EMPTY) {
		DBG_PRINT("Freeing superblock %p with desc %p (count %hu)\n", sb, desc, desc->anchor.load().count);

		base_md->small_sb_retire(sb);
		base_md->remove_empty_desc(heap, desc);
	} 
	else if (oldanchor.state == FULL) {
		DBG_PRINT("Puting superblock %p to PARTIAL heap\n", sb);
		base_md->heap_put_partial(desc);
	}
}

//return the old i-th root, if exists.
void* pmmalloc::set_root(void* ptr, uint64_t i){
	return base_md->set_root(ptr,i);
}

//return the current i-th root, or nullptr if not exist.
void* pmmalloc::get_root(uint64_t i){
	return base_md->get_root(i);
}