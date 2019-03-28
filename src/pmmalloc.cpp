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

//required by linker
pmmalloc* pmmalloc::obj = nullptr;
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

inline void* pmmalloc::__p_malloc(size_t sz){
	return obj->do_malloc(sz);
}

inline void pmmalloc::__p_free(void* ptr){
	obj->do_free(ptr);
}

//manually request to collect garbage
bool pmmalloc::__collect(){
	//TODO
	return true;
}

//return the old i-th root, if exists.
inline void* pmmalloc::__set_root(void* ptr, uint64_t i){
	return base_md->set_root(ptr,i);
}

//return the current i-th root, or nullptr if not exist.
inline void* pmmalloc::__get_root(uint64_t i){
	return base_md->get_root(i);
}