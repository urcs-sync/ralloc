#include "TCache.hpp"

thread_local TCacheBin pmmalloc::t_cache[MAX_SZ_IDX];

void TCacheBin::push_block(char* block)
{
	// block has at least sizeof(char*)
	assert(pmmalloc::initialized&&"PM should be initialized first.");
	assert(pmmalloc::mgr&&"mgr should not be NULL.");
	*(pptr<char>*)block = _block==0 ? nullptr : (char*)(_block + (uint64_t)pmmalloc::mgr->base_addr);
	_block = (uint64_t)block - (uint64_t)pmmalloc::mgr->base_addr;
	_block_num++;
}

void TCacheBin::push_list(char* block, uint32_t length)
{
	// caller must ensure there's no available block
	// this op is only used to fill empty cache
	assert(pmmalloc::initialized&&"PM should be initialized first.");
	assert(pmmalloc::mgr&&"mgr should not be NULL.");
	assert(_block_num == 0);

	_block = (uint64_t)block - (uint64_t)pmmalloc::mgr->base_addr;
	_block_num = length;
}

char* TCacheBin::pop_block()
{
	// caller must ensure there's an available block
	assert(pmmalloc::initialized&&"PM should be initialized first.");
	assert(pmmalloc::mgr&&"mgr should not be NULL.");
	assert(_block_num > 0);

	char* ret = (char*)(_block + (uint64_t)pmmalloc::mgr->base_addr);
	char* next = (char*)(*(pptr<char>*)(_block + (uint64_t)pmmalloc::mgr->base_addr));
	_block = next == nullptr ? 0 : (uint64_t) next - (uint64_t)pmmalloc::mgr->base_addr;
	_block_num--;
	return ret;
}

void TCacheBin::pop_list(char* block, uint32_t length)
{
	assert(pmmalloc::initialized&&"PM should be initialized first.");
	assert(pmmalloc::mgr&&"mgr should not be NULL.");
	assert(_block_num >= length);

	_block = block==nullptr? 0: (uint64_t)block - (uint64_t)pmmalloc::mgr->base_addr;
	_block_num -= length;
}
