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
pmmalloc::pmmalloc(string id, uint64_t thd_num = MAX_THREADS) : 
	thread_num(thd_num), 
	free_desc(thd_num) {
	bool restart = false;

	//TODO: find all heap files with this id to determine the value of restart, and assign appropriate path to filepath
	if(restart){
		mgr = new RegionManager(filepath);
		void* hstart = mgr->__fetch_heap_start();
		base_md = (BaseMeta*) hstart;
		base_md->mgr = mgr;
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
	return true;
}

void* pmmalloc::malloc(size_t sz, vector<void*>(*f) = nullptr){
	return malloc(sz);
}

void pmmalloc::free(void* ptr){
	free(ptr);
}

//return the old i-th root, if exists.
void* pmmalloc::set_root(void* ptr, uint64_t i){
	return nullptr;
}

//return the current i-th root, or nullptr if not exist.
void* pmmalloc::get_root(uint64_t i){
	return nullptr;
}