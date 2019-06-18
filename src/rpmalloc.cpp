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
#include "rpmalloc.hpp"

#include <string>
#include <atomic>
#include <vector>
#include <algorithm>

#include "RegionManager.hpp"
#include "BaseMeta.hpp"
// #include "thread_util.hpp"

using namespace std;

namespace rpmalloc{
	bool initialized = false;
	std::string filepath;
	// uint64_t thread_num;
	/* persistent metadata and their layout */
	BaseMeta* base_md;
	//GC
	Regions* _rgs;
};
using namespace rpmalloc;


/* 
 * mmap the existing heap file corresponding to id. aka restart,
 * 		and if multiple heaps exist, print out and let user select;
 * if such a heap doesn't exist, create one. aka start.
 * id is the distinguishable identity of applications.
 */
void RP_init(char* _id, uint64_t size){
	string id(_id);
	// thread_num = thd_num;
	filepath = HEAPFILE_PREFIX + id;
	assert(sizeof(Descriptor) == DESCSIZE); // check desc size
	assert(size >= MAX_SB_REGION_SIZE); // ensure user input is >=MAX_SB_REGION_SIZE
	uint64_t num_sb = size/SBSIZE;
	_rgs = new Regions();
	for(int i=0; i<LAST_IDX;i++){
	switch(i){
	case DESC_IDX:
		_rgs->create(filepath+"_desc", num_sb*DESCSIZE, true, true);
		break;
	case SB_IDX:
		_rgs->create(filepath+"_sb", num_sb*SBSIZE, true, false);
		break;
	case META_IDX:
		base_md = _rgs->create_for<BaseMeta>(filepath+"_basemd", sizeof(BaseMeta), true);
		break;
	} // switch
	}
	initialized = true;
}

void RP_close(){
	base_md->cleanup();
	delete _rgs;
	initialized = false;
}

//manually request to collect garbage
int RP_collect(){
	//TODO
	return 1;
}

void* RP_malloc(size_t sz){
	if(UNLIKELY(initialized==false)){
		RP_init("no_explicit_init");
	}
	return base_md->do_malloc(sz);
}

void RP_free(void* ptr){
	if(UNLIKELY(initialized==false)){
		RP_init("no_explicit_init");
	}
	base_md->do_free(ptr);
}
void* RP_set_root(void* ptr, uint64_t i){
	if(UNLIKELY(initialized==false)){
		RP_init("no_explicit_init");
	}
	return base_md->set_root(ptr,i);
}
void* RP_get_root(uint64_t i){
	if(UNLIKELY(initialized==false)){
		RP_init("no_explicit_init");
	}
	return base_md->get_root(i);
}
