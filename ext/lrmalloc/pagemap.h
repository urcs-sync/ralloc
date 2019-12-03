/*
 * Copyright (C) 2019 Ricardo Leite. All rights reserved.
 * Licenced under the MIT licence. See COPYING file in the project root for details.
 */

#ifndef __PAGEMAP_H
#define __PAGEMAP_H

#include <atomic>

#include "defines.h"
#include "size_classes.h"
#include "log.h"

// assuming x86-64, for now
// which uses 48 bits for addressing (e.g high 16 bits ignored)
// can ignore the bottom 12 bits (lg of page)
// insignificant high bits
#define PM_NHS 14
// insignificant low bits
#define PM_NLS LG_PAGE
// significant middle bits
#define PM_SB (64 - PM_NHS - PM_NLS)
// to get the key from a address
// 1. shift to remove insignificant low bits
// 2. apply mask of middle significant bits
#define PM_KEY_SHIFT PM_NLS
#define PM_KEY_MASK ((1ULL << PM_SB) - 1)

struct Descriptor;
// associates metadata to each allocator page
// implemented with a static array, but can also be implemented
//  with a multi-level radix tree

#define SC_MASK ((1ULL << 6) - 1)

// contains metadata per page
// *has* to be the size of a single word
struct PageInfo
{
private:
    // descriptor
    Descriptor* _desc;
    // size class
    // stealing bits from desc to store size class
    // desc is aligned to at least 64 bytes, so 6 bits to steal
    // which is the same as LG_MAX_SIZE_IDX
    // size_t scIdx : LG_MAX_SIZE_IDX;

public:
    void Set(Descriptor* desc, size_t scIdx);
    Descriptor* GetDesc() const;
    size_t GetScIdx() const;
};

inline void PageInfo::Set(Descriptor* desc, size_t scIdx)
{
    ASSERT(((size_t)desc & SC_MASK) == 0);
    ASSERT(scIdx < MAX_SZ_IDX);

    _desc = (Descriptor*)((size_t)desc | scIdx);
}

inline Descriptor* PageInfo::GetDesc() const
{
    return (Descriptor*)((size_t)_desc & ~SC_MASK);
}

inline size_t PageInfo::GetScIdx() const
{
    return ((size_t)_desc & SC_MASK);
}

#define PM_SZ ((1ULL << PM_SB) * sizeof(PageInfo))

static_assert(sizeof(PageInfo) == sizeof(uint64_t), "Invalid PageInfo size");

// lock free page map
class PageMap
{
public:
    // must be called before any GetPageInfo/SetPageInfo calls
    void Init();

    PageInfo GetPageInfo(char* ptr);
    void SetPageInfo(char* ptr, PageInfo info);

private:
    size_t AddrToKey(char* ptr) const;

private:
    // array based impl
    std::atomic<PageInfo>* _pagemap = { nullptr };
};

inline size_t PageMap::AddrToKey(char* ptr) const
{
    size_t key = ((size_t)ptr >> PM_KEY_SHIFT) & PM_KEY_MASK;
    return key;
}

inline PageInfo PageMap::GetPageInfo(char* ptr)
{
    size_t key = AddrToKey(ptr);
    return _pagemap[key].load();
}

inline void PageMap::SetPageInfo(char* ptr, PageInfo info)
{
    size_t key = AddrToKey(ptr);
    _pagemap[key].store(info);
}

extern PageMap sPageMap;

#endif // __PAGEMAP_H

