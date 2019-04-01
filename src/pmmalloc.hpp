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

void PM_init(std::string id, uint64_t thd_num = MAX_THREADS);
void PM_close();
bool PM_collect();
template<class T = void>
T* PM_malloc(size_t sz);
void PM_free(void* ptr);
void* PM_set_root(void* ptr, uint64_t i);
void* PM_get_root(uint64_t i);
#define PM_pthread_create(thd, attr, f, arg) pm_thread_create(thd, attr, f, arg)

/*
 ************class pmmalloc************
 * This is a persistent lock-free allocator based on LRMalloc.
 *
 * Function:
 * 		_init(string id, uint64_t thd_num):
 * 			Construct the singleton with id to decide where the data 
 * 			maps to. If the file exists, it tries to restart; otherwise,
 * 			it starts from scratch.
 * 		_close():
 * 			Shutdown the allocator by cleaning up free list 
 * 			and RegionManager pointer, but BaseMeta data will
 * 			preserve for remapping during restart.
 * 		T* _p_malloc<T>(size_t sz):
 * 			Malloc a block with size sz and type T.
 * 			Currently it only supports small allocation (<=MAX_SMALLSIZE).
 * 			If T is not void, sz will be ignored.
 * 		void _p_free(void* ptr):
 * 			Free the block pointed by ptr.
 * 		void* _set_root(void* ptr, uint64_t i):
 * 			Set i-th root to ptr, and return old i-th root if any.
 * 		void* _get_root(uint64_t i):
 * 			Return i-th root.
 * 		bool _collect(): TODO
 * 			Manually bring pmmalloc offline and do garbage collection
 * 
 * Note: Main data is stored in *base_md and is mapped to 
 * filepath, which is $(HEAPFILE_PREFIX)$(id).


 * It's from paper: 
 * 		LRMalloc: A Modern and Competitive Lock-Free Dynamic Memory Allocator
 * by 
 * 		Ricardo Leite and Ricardo Rocha
 *
 * p_malloc() and p_free() have large portion of code from the open source
 * project https://github.com/ricleite/lrmalloc.
 *
 * Adapted and reimplemented by:
 * 		Wentao Cai (wcai6@cs.rochester.edu)
 *
 */

class pmmalloc{
public:
	static void _init(std::string id, uint64_t thd_num);
	static void _close();
	static bool _collect();
	template<class T>
	static T* _p_malloc(size_t sz);
	static void _p_free(void* ptr);
	static void* _set_root(void* ptr, uint64_t i);
	static void* _get_root(uint64_t i);

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

inline void PM_init(std::string id, uint64_t thd_num){
	return pmmalloc::_init(id, thd_num);
}
inline void PM_close(){
	return pmmalloc::_close();
}
inline bool PM_collect(){
	return pmmalloc::_collect();
}

template<class T>
inline T* PM_malloc(size_t sz){
	return pmmalloc::_p_malloc<T>(sz);
}
inline void PM_free(void* ptr) {
	return pmmalloc::_p_free(ptr);
}
inline void* PM_set_root(void* ptr, uint64_t i){
	return pmmalloc::_set_root(ptr, i);
}
inline void* PM_get_root(uint64_t i) {
	return pmmalloc::_get_root(i);
}

template<class T>
T* pmmalloc::_p_malloc(size_t sz){
	assert(obj!=nullptr&&"pmmalloc isn't initialized!");
	return (T*)obj->__p_malloc(sizeof(T));
}
template<>
inline void* pmmalloc::_p_malloc<void>(size_t sz){
	assert(obj!=nullptr&&"pmmalloc isn't initialized!");
	return obj->__p_malloc(sz);
}
#endif /* _PMMALLOC_HPP_ */