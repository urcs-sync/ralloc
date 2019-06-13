#include "TCache.hpp"

using namespace rpmalloc;
thread_local TCacheBin rpmalloc::t_cache[MAX_SZ_IDX];

void TCacheBin::push_block(char* block)
{
	// block has at least sizeof(char*)
	assert(initialized&&"PM should be initialized first.");
	assert(_rgs&&"_rgs should not be NULL.");
	*(pptr<char>*)block = _block==0 ? nullptr : _rgs->translate(META_IDX,_block);
	_block = _rgs->untranslate(META_IDX,block);
	_block_num++;
}

void TCacheBin::push_list(char* block, uint32_t length)
{
	// caller must ensure there's no available block
	// this op is only used to fill empty cache
	assert(initialized&&"PM should be initialized first.");
	assert(_rgs&&"_rgs should not be NULL.");
	assert(_block_num == 0);

	_block = _rgs->untranslate(META_IDX,block);
	_block_num = length;
}

char* TCacheBin::pop_block()
{
	// caller must ensure there's an available block
	assert(initialized&&"PM should be initialized first.");
	assert(_rgs&&"_rgs should not be NULL.");
	assert(_block_num > 0);

	char* ret = _rgs->translate(META_IDX,_block);
	char* next = (char*)(*(pptr<char>*)ret);
	_block = next == nullptr ? 0 : _rgs->untranslate(META_IDX,next);
	_block_num--;
	return ret;
}

void TCacheBin::pop_list(char* block, uint32_t length)
{
	assert(initialized&&"PM should be initialized first.");
	assert(_rgs&&"_rgs should not be NULL.");
	assert(_block_num >= length);

	_block = block==nullptr? 0: _rgs->untranslate(META_IDX,block);
	_block_num -= length;
}
