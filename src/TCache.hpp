
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
 * This defines thread-local cache, and is reconstructible.
 * As a result, no need to flush at all.
 * During the exit, all cached blocks will be given back to superblocks.
 * 
 * The head (_block) of each cache list uses absolute address while
 * the list itself is linked by pptr since block free list is linked by pptr.
 *
 * In the destructor of TCacheBin, all blocks will be flushed back to their 
 * superblock as long as initialized is true.
 * 
 * Note by Wentao Cai (wcai6@cs.rochester.edu)
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

namespace rpmalloc{
	extern void public_flush_cache();
}
struct TCaches
{
	TCacheBin t_cache[MAX_SZ_IDX];
	TCaches():t_cache(){};
	~TCaches(){
		rpmalloc::public_flush_cache();
	}
};

/* thread-local cache */
namespace rpmalloc{
	extern thread_local TCaches t_caches;
}
#endif // __TCACHE_H_

