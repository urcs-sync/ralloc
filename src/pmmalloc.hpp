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
void* PM_malloc(size_t sz);
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

namespace pmmalloc{
	extern bool initialized;
	extern std::string filepath;
	extern uint64_t thread_num;
	/* manager to map, remap, and unmap the heap */
	extern RegionManager* mgr;//initialized when pmmalloc constructs
	/* persistent metadata and their layout */
	extern BaseMeta* base_md;
	//GC
};
#endif /* _PMMALLOC_HPP_ */