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

#include <stddef.h>
#include <stdint.h>
#include "pm_config.hpp"

#ifdef __cplusplus
extern "C" int RP_init(const char* _id, uint64_t size = MIN_SB_REGION_SIZE);
#include "BaseMeta.hpp"
namespace rpmalloc{
	extern bool initialized;
	/* persistent metadata and their layout */
	extern BaseMeta* base_md;
};
template<class T>
void* RP_set_root(T* ptr, uint64_t i){
	if(UNLIKELY(rpmalloc::initialized==false)){
		RP_init("no_explicit_init");
	}
	return rpmalloc::base_md->set_root(ptr,i);
}
extern "C"{
#else
// This is a version for pure c only
void* RP_set_root_c(void* ptr, uint64_t i);
int RP_init(const char* _id, uint64_t size);
#endif

/* return 1 if it's a restart, otherwise 0. */
void RP_close();
void* RP_malloc(size_t sz);
void RP_free(void* ptr);
void* RP_get_root(uint64_t i);

#ifdef __cplusplus
}
#endif

#define RP_pthread_create(thd, attr, f, arg) pm_thread_create(thd, attr, f, arg)
/*
 ************class rpmalloc************
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

#endif /* _PMMALLOC_HPP_ */