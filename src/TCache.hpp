/*
 * Copyright (C) 2019 University of Rochester. All rights reserved.
 * Licenced under the MIT licence. See LICENSE file in the project root for
 * details. 
 */

/*
 * Copyright (C) 2018 Ricardo Leite
 * Licenced under the MIT licence. This file shares some portion from
 * LRMalloc(https://github.com/ricleite/lrmalloc) and its copyright 
 * is retained. See LICENSE for details about MIT License.
 */

#ifndef __TCACHE_H_
#define __TCACHE_H_

#include "pm_config.hpp"
#include "pfence_util.h"
#include "SizeClass.hpp"
#include "pptr.hpp"
#include "RegionManager.hpp"

/*
 * This is from LRMALLOC:
 * https://github.com/ricleite/lrmalloc
 * 
 * This defines thread-local cache, using C++ TLS.
 * During normal exit, all cached blocks will be given back to superblocks.
 * 
 * The head (_block) of each cache list uses absolute address while
 * the list itself is linked by pptr since block free list is linked by pptr.
 *
 * In the destructor of TCacheBin, all blocks will be flushed back to their 
 * superblock as long as ralloc::initialized is true.
 * 
 * Wentao Cai (wcai6@cs.rochester.edu)
 */

struct TCaches;

struct TCacheBin
{
private:
	char* _block;//absolute address of block
	uint32_t _block_num;

public:
	// common, fast ops
	void push_block(char* block);
	// push block list, cache *must* be empty
	void push_list(char* block, uint32_t length);

	char* pop_block(); // can return nullptr
	// manually popped list of blocks and now need to update cache
	// `block` is the new head
	void pop_list(char* block, uint32_t length);
	char* peek_block() const { return _block; }

	uint32_t get_block_num() const { return _block_num; }
	TCacheBin() noexcept:_block(nullptr), _block_num(0) {};
	// slow operations like fill/flush handled in cache user
};

namespace ralloc{
	extern void public_flush_cache();
}
struct TCaches
{
	TCacheBin t_cache[MAX_SZ_IDX];
	TCaches():t_cache(){};
	~TCaches(){
		ralloc::public_flush_cache();
	}
};

/* thread-local cache */
namespace ralloc{
	extern thread_local TCaches t_caches;
}
#endif // __TCACHE_H_

