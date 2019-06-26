#include "TCache.hpp"

using namespace rpmalloc;
thread_local TCaches rpmalloc::t_caches;

void TCacheBin::push_block(char* block)
{
	// block has at least sizeof(char*)
	*(pptr<char>*)block = _block;
	_block = block;
	_block_num++;
}

void TCacheBin::push_list(char* block, uint32_t length)
{
	// caller must ensure there's no available block
	// this op is only used to fill empty cache
	assert(_block_num == 0);

	_block = block;
	_block_num = length;
}

char* TCacheBin::pop_block()
{
	// caller must ensure there's an available block
	assert(_block_num > 0);

	char* ret = _block;
	char* next = (char*)(*(pptr<char>*)ret);
	_block = next;
	_block_num--;
	return ret;
}

void TCacheBin::pop_list(char* block, uint32_t length)
{
	assert(_block_num >= length);

	_block = block;
	_block_num -= length;
}
