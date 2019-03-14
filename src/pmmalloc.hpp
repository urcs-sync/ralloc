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

using namespace std;

/*
 ************class pmmalloc************
 * This is a persistent lock-free allocator based on 
 * Maged Michael's lock-free allocator.
 *
 * Function:
 * 		pmmalloc(string id):
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
	pmmalloc(string id, uint64_t thd_num = MAX_THREADS); // start/restart the heap by the application id.
	~pmmalloc(); // destructor to close the heap
	void* p_malloc(size_t sz, vector<void*>(*f) = nullptr);
	void p_free(void* ptr);
	void* set_root(void* ptr, uint64_t i);//return the old i-th root
	void* get_root(uint64_t i);
	bool collect();

private:
	string filepath;
	uint64_t thread_num;
	/* manager to map, remap, and unmap the heap */
	RegionManager* mgr;//initialized when pmmalloc constructs
	/* persistent metadata and their layout */
	BaseMeta* base_md;

	//GC
};

#endif /* _PMMALLOC_HPP_ */