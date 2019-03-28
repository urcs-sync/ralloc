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
#ifndef _PMMALLOC_HPP_
#define _PMMALLOC_HPP_

#include <string>
#include <atomic>
#include <vector>

#include "pm_config.hpp"

#include "RegionManager.hpp"
#include "BaseMeta.hpp"
#include "thread_util.hpp"

#define PM_init(id, thd_num) pmmalloc::_init(id, thd_num)
#define PM_malloc(sz) pmmalloc::_p_malloc(sz)
#define PM_free(ptr) pmmalloc::_p_free(ptr)
#define PM_set_root(ptr, i) pmmalloc::_set_root(ptr, i)
#define PM_get_root(i) pmmalloc::_get_root(i)
#define PM_collect() pmmalloc::_collect()
#define PM_close() pmmalloc::_close()
#define PM_pthread_create(thd, attr, f, arg) pm_thread_create(thd, attr, f, arg)

/*
 ************class pmmalloc************
 * This is a persistent lock-free allocator based on 
 * Maged Michael's lock-free allocator.
 *
 * Function:
 * 		pmmalloc(string id, uint64_t thd_num):
 * 			Constructor with id to decide where the data maps to.
 * 			If the file exists, it tries to restart; otherwise,
 * 			it starts from scratch.
 * 		~pmmalloc():
 * 			Shutdown the allocator by cleaning up free list 
 * 			and RegionManager pointer, but BaseMeta data will
 * 			be reserved in order to remap while restarting.
 * 		void* p_malloc(size_t sz, vector<void*>(*f)):
 * 			Malloc a block with size sz.
 * 			Currently it only supports small allocation (<=MAX_SMALLSIZE).
 * 			Filter function f is to specify pointers the block contains
 * 			for a quick garbage collection. (TODO)
 * 		void p_free(void* ptr):
 * 			Free the block pointed by ptr.
 * 		void* set_root(void* ptr, uint64_t i):
 * 			Set i-th root to ptr, and return old i-th root if any.
 * 		void* get_root(uint64_t i):
 * 			Return i-th root.
 * 		bool collect(): TODO
 * 			Manually bring pmmalloc offline and do garbage collection
 * 
 * Note: Main data is stored in *base_md and is mapped to 
 * filepath, which is $(HEAPFILE_PREFIX)$(id).


 * It's from paper: 
 * 		Scalable Lock-Free Dynamic Memory Allocation
 * by 
 * 		Maged M. Michael
 *
 * p_malloc() and p_free() have large portion of code from the open source
 * project https://github.com/scotts/michael.
 *
 * Adapted and reimplemented by:
 * 		Wentao Cai (wcai6@cs.rochester.edu)
 *
 */

class pmmalloc{
public:
	static inline void _init(std::string id, uint64_t thd_num = MAX_THREADS){
		obj = new pmmalloc(id, thd_num); 
	}
	static inline void* _p_malloc(size_t sz){
		assert(obj!=nullptr&&"pmmalloc isn't initialized!");
		return obj->__p_malloc(sz);
	}
	static inline void _p_free(void* ptr){
		assert(obj!=nullptr&&"pmmalloc isn't initialized!");
		obj->__p_free(ptr);
	}
	static inline void* _set_root(void* ptr, uint64_t i){
		assert(obj!=nullptr&&"pmmalloc isn't initialized!");
		return obj->__set_root(ptr,i);
	}
	static inline void* _get_root(uint64_t i){
		assert(obj!=nullptr&&"pmmalloc isn't initialized!");
		return obj->__get_root(i);
	}
	static inline bool _collect(){
		assert(obj!=nullptr&&"pmmalloc isn't initialized!");
		return obj->__collect();
	}
	static inline void _close(){
		delete obj;obj = nullptr; 
	}

private:
	static pmmalloc* obj; // singleton
	pmmalloc(std::string id, uint64_t thd_num); // start/restart the heap by the application id.
	~pmmalloc(); // destructor to close the heap
	void* __p_malloc(size_t sz);
	void __p_free(void* ptr);
	void* __set_root(void* ptr, uint64_t i);//return the old i-th root
	void* __get_root(uint64_t i);
	bool __collect();
	std::string filepath;
	uint64_t thread_num;
	/* manager to map, remap, and unmap the heap */
	RegionManager* mgr;//initialized when pmmalloc constructs
	/* persistent metadata and their layout */
	BaseMeta* base_md;
	//GC
};

#endif /* _PMMALLOC_HPP_ */