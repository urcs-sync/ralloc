#include "pmmalloc.hpp"

#include <ctime>

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
pmmalloc::pmmalloc(string id, uint64_t thd_num) : 
	thread_num(thd_num){
	bool restart = false;

	//TODO: find all heap files with this id to determine the value of restart, and assign appropriate path to filepath
	if(restart){
		mgr = new RegionManager(filepath);
		void* hstart = mgr->__fetch_heap_start();
		base_md = (BaseMeta*) hstart;
		base_md->set_mgr(mgr);
		//collect if the heap is dirty
	} else {
		/* RegionManager init */
		filepath = HEAPFILE_PREFIX + id + "_" + 
			to_string(time(nullptr));
		mgr = new RegionManager(filepath);
		int res = mgr->__nvm_region_allocator((void**)&base_md,sizeof(void*),sizeof(BaseMeta));
		if(res!=0) assert(0&&"mgr allocation fails!");
		mgr->__store_heap_start(base_md);
		new (base_md) BaseMeta(mgr, thd_num);
	}
}

//destructor aka close
pmmalloc::~pmmalloc(){
	//close
	delete mgr;
}

//manually request to collect garbage
bool pmmalloc::collect(){
	//TODO
	return true;
}

void* pmmalloc::p_malloc(size_t sz, vector<void*>(*f)){
	//TODO: put f in each block
	Procheap *heap;
	void* addr;
#ifdef DEBUG
	fprintf(stderr, "malloc() sz %lu\n", sz);
	fflush(stderr);
#endif
	// Use sz and thread id to find heap.
	heap = base_md->find_heap(sz);

	if (!heap) {
		// Large block
		addr = base_md->alloc_large_block(sz);
#ifdef DEBUG
		fprintf(stderr, "Large block allocation: %p\n", addr);
		fflush(stderr);
#endif
		return addr;
	}

	while(1) { 
		addr = base_md->malloc_from_active(heap);
		if (addr) {
#ifdef DEBUG
			fprintf(stderr, "malloc() return MallocFromActive %p\n", addr);
			fflush(stderr);
#endif
			return addr;
		}
		addr = base_md->malloc_from_partial(heap);
		if (addr) {
#ifdef DEBUG
			fprintf(stderr, "malloc() return MallocFromPartial %p\n", addr);
			fflush(stderr);
#endif
			return addr;
		}
		addr = base_md->malloc_from_newsb(heap);
		if (addr) {
#ifdef DEBUG
			fprintf(stderr, "malloc() return MallocFromNewSB %p\n", addr);
			fflush(stderr);
#endif
			return addr;
		}
	} 
}

void pmmalloc::p_free(void* ptr){
	Descriptor* desc;
	void* sb;
	Anchor oldanchor, newanchor;
	Procheap* heap = NULL;
#ifdef DEBUG
	fprintf(stderr, "Calling my free %p\n", ptr);
	fflush(stderr);
#endif

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
#ifdef DEBUG
		fprintf(stderr, "Freeing large block\n");
		fflush(stderr);
#endif
		base_md->sb_retire(ptr, *((uint64_t *)(ptr + TYPE_SIZE)));
		return;
	}
	desc = *((Descriptor**)((uint64_t)ptr + TYPE_SIZE));
	
	sb = desc->sb;
	oldanchor = desc->anchor;
	do { 
		newanchor = oldanchor;

		*((uint64_t*)ptr) = oldanchor.avail;
		newanchor.avail = ((uint64_t)ptr - (uint64_t)sb) / desc->sz;

		if (oldanchor.state == FULL) {
#ifdef DEBUG
			fprintf(stderr, "Marking superblock %p as PARTIAL\n", sb);
			fflush(stderr);
#endif
			newanchor.state = PARTIAL;
		}

		if (oldanchor.count == desc->maxcount - 1) {
			heap = desc->heap;
			INSTRFENCE;// instruction fence.
#ifdef DEBUG
			fprintf(stderr, "Marking superblock %p as EMPTY; count %d\n", sb, oldanchor.count);
			fflush(stderr);
#endif
			newanchor.state = EMPTY;
		} 
		else {
			++newanchor.count;
		}
		// memory fence.
		FLUSHFENCE;
	} while (!desc->anchor.compare_exchange_weak(oldanchor, newanchor));
	FLUSH(&desc->anchor);
	FLUSHFENCE;
	if (newanchor.state == EMPTY) {
#ifdef DEBUG
		fprintf(stderr, "Freeing superblock %p with desc %p (count %hu)\n", sb, desc, desc->anchor.load().count);
		fflush(stderr);
#endif

		base_md->sb_retire(sb, heap->sc->sbsize);
		base_md->remove_empty_desc(heap, desc);
	} 
	else if (oldanchor.state == FULL) {
#ifdef DEBUG
		fprintf(stderr, "Puting superblock %p to PARTIAL heap\n", sb);
		fflush(stderr);
#endif
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