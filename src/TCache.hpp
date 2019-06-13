
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
 *
 * Note by Wentao Cai (wcai6@cs.rochester.edu)
 */
namespace rpmalloc{
	extern Regions* _rgs;
	extern bool initialized;
}

struct TCacheBin
{
private:
	uint64_t _block;
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
	char* peek_block() const { return _block==0 ? nullptr : rpmalloc::_rgs->translate(META_IDX,_block); }

	uint32_t get_block_num() const { return _block_num; }
	TCacheBin() noexcept:_block(0), _block_num(0) {};
	// slow operations like fill/flush handled in cache user
};

/* thread-local cache */
namespace rpmalloc{
	extern thread_local TCacheBin t_cache[MAX_SZ_IDX]
		__attribute__((aligned(CACHELINE_SIZE)));
}
#endif // __TCACHE_H_

