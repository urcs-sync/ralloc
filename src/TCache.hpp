
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
	uint32_t _block_num = 0;

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

	// slow operations like fill/flush handled in cache user
};

inline void TCacheBin::push_block(char* block)
{
	// block has at least sizeof(char*)
	*(char**)block = _block;
	TFLUSH(block);
	TFLUSHFENCE;
	_block = block;
	_block_num++;
	TFLUSH(&_block);
	TFLUSH(&_block_num);
	TFLUSHFENCE;
}

inline void TCacheBin::push_list(char* block, uint32_t length)
{
	// caller must ensure there's no available block
	// this op is only used to fill empty cache
	assert(_block_num == 0);

	_block = block;
	_block_num = length;
	TFLUSH(&_block);
	TFLUSH(&_block_num);
	TFLUSHFENCE;
}

inline char* TCacheBin::pop_block()
{
	// caller must ensure there's an available block
	assert(_block_num > 0);

	char* ret = _block;
	_block = *(char**)_block;
	_block_num--;
	TFLUSH(&_block);
	TFLUSH(&_block_num);
	TFLUSHFENCE;
	return ret;
}

inline void TCacheBin::pop_list(char* block, uint32_t length)
{
	assert(_block_num >= length);

	_block = block;
	_block_num -= length;
	TFLUSH(&_block);
	TFLUSH(&_block_num);
	TFLUSHFENCE;
}

#endif // __TCACHE_H_

