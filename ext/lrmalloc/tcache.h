/*
 * Copyright (C) 2019 Ricardo Leite. All rights reserved.
 * Licenced under the MIT licence. See COPYING file in the project root for details.
 */

#ifndef __TCACHE_H_
#define __TCACHE_H_

#include "defines.h"
#include "size_classes.h"
#include "log.h"

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
    _block = block;
    _blockNum++;
}

inline void TCacheBin::PushList(char* block, uint32_t length)
{
    // caller must ensure there's no available block
    // this op is only used to fill empty cache
    ASSERT(_blockNum == 0);

    _block = block;
    _blockNum = length;
}

inline char* TCacheBin::PopBlock()
{
    // caller must ensure there's an available block
    ASSERT(_blockNum > 0);

    char* ret = _block;
    _block = *(char**)_block;
    _blockNum--;
    return ret;
}

inline void TCacheBin::PopList(char* block, uint32_t length)
{
    ASSERT(_blockNum >= length);

    _block = block;
    _blockNum -= length;
}

// use tls init exec model
extern __thread TCacheBin TCache[MAX_SZ_IDX]
    LFMALLOC_TLS_INIT_EXEC LFMALLOC_CACHE_ALIGNED;

#endif // __TCACHE_H_

