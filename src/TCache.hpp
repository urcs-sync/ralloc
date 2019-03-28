
#ifndef __TCACHE_H_
#define __TCACHE_H_

#include "pm_config.hpp"
#include "pfence_util.h"
#include "SizeClass.hpp"

/*
 * This is from LRMALLOC:
 * https://github.com/ricleite/lrmalloc
 * 
 * This defines thread-local cache, and is reconstructible.
 *
 * Note by Wentao Cai (wcai6@cs.rochester.edu)
 */

struct TCacheBin
{
private:
	char* _block = nullptr;
	uint32_t _blockNum = 0;

public:
	// common, fast ops
	void PushBlock(char* block);
	// push block list, cache *must* be empty
	void PushList(char* block, uint32_t length);

	char* PopBlock(); // can return nullptr
	// manually popped list of blocks and now need to update cache
	// `block` is the new head
	void PopList(char* block, uint32_t length);
	char* PeekBlock() const { return _block; }

	uint32_t GetBlockNum() const { return _blockNum; }

	// slow operations like fill/flush handled in cache user
};

inline void TCacheBin::PushBlock(char* block)
{
	// block has at least sizeof(char*)
	*(char**)block = _block;
	TFLUSH(block);
	TFLUSHFENCE;
	_block = block;
	_blockNum++;
	TFLUSH(&_block);
	TFLUSH(&_blockNum);
	TFLUSHFENCE;
}

inline void TCacheBin::PushList(char* block, uint32_t length)
{
	// caller must ensure there's no available block
	// this op is only used to fill empty cache
	assert(_blockNum == 0);

	_block = block;
	_blockNum = length;
	TFLUSH(&_block);
	TFLUSH(&_blockNum);
	TFLUSHFENCE;
}

inline char* TCacheBin::PopBlock()
{
	// caller must ensure there's an available block
	assert(_blockNum > 0);

	char* ret = _block;
	_block = *(char**)_block;
	_blockNum--;
	TFLUSH(&_block);
	TFLUSH(&_blockNum);
	TFLUSHFENCE;
	return ret;
}

inline void TCacheBin::PopList(char* block, uint32_t length)
{
	assert(_blockNum >= length);

	_block = block;
	_blockNum -= length;
	TFLUSH(&_block);
	TFLUSH(&_blockNum);
	TFLUSHFENCE;
}

#endif // __TCACHE_H_

