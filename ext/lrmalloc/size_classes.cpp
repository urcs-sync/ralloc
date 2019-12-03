/*
 * Copyright (C) 2019 Ricardo Leite. All rights reserved.
 * Licenced under the MIT licence. See COPYING file in the project root for details.
 */

#include "defines.h"
#include "size_classes.h"
#include "lrmalloc.h"
#include "log.h"

#define SIZE_CLASS_bin_yes(blockSize, pages) \
    { blockSize, pages * PAGE, 0, 0 },
#define SIZE_CLASS_bin_no(blockSize, pages)

#define SC(index, lg_grp, lg_delta, ndelta, psz, bin, pgs, lg_delta_lookup) \
    SIZE_CLASS_bin_##bin((1U << lg_grp) + (ndelta << lg_delta), pgs)

SizeClassData SizeClasses[MAX_SZ_IDX] = {
    { 0, 0 ,0, 0},
    SIZE_CLASSES
};

size_t SizeClassLookup[MAX_SZ + 1] = { 0 };

void InitSizeClass()
{
    // each superblock has to contain several blocks
    // and it has to contain blocks *perfectly*
    //  e.g no space left after last block
    for (size_t scIdx = 1; scIdx < MAX_SZ_IDX; ++scIdx)
    {
        SizeClassData& sc = SizeClasses[scIdx];
        size_t blockSize = sc.blockSize;
        size_t sbSize = sc.sbSize;
        // size class large enough to store several elements
        if (sbSize > blockSize && (sbSize % blockSize) == 0)
            continue; // skip

        // increase superblock size so it can hold >1 elements
        while (blockSize >= sbSize)
            sbSize += sc.sbSize;

        sc.sbSize = sbSize;
    }

    // increase superblock size if need
    for (size_t scIdx = 1; scIdx < MAX_SZ_IDX; ++scIdx)
    {
        SizeClassData& sc = SizeClasses[scIdx];
        size_t sbSize = sc.sbSize;
        // 64KB
        while (sbSize < (16 * PAGE))
            sbSize += sc.sbSize;

        sc.sbSize = sbSize;
    }

    // fill blockNum and cacheBlockNum
    for (size_t scIdx = 1; scIdx < MAX_SZ_IDX; ++scIdx)
    {
        SizeClassData& sc = SizeClasses[scIdx];
        // blockNum calc
        sc.blockNum = sc.sbSize / sc.blockSize;
        // cacheBlockNum calc
        sc.cacheBlockNum = sc.blockNum * 1;
        ASSERT(sc.blockNum > 0);
        ASSERT(sc.blockNum < MAX_BLOCK_NUM);
        ASSERT(sc.blockNum >= sc.cacheBlockNum);
    }

    // first size class reserved for large allocations
    size_t lookupIdx = 0;
    for (size_t scIdx = 1; scIdx < MAX_SZ_IDX; ++scIdx)
    {
        SizeClassData const& sc = SizeClasses[scIdx];
        size_t blockSize = sc.blockSize;
        while (lookupIdx <= blockSize)
        {
            SizeClassLookup[lookupIdx] = scIdx;
            ++lookupIdx;
        } 
    }
}


