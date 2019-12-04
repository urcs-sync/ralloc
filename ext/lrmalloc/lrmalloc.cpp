/*
 * Copyright (C) 2019 Ricardo Leite. All rights reserved.
 * Licenced under the MIT licence. See COPYING file in the project root for details.
 */

#include <cassert>
#include <cstring>
#include <algorithm>
#include <atomic>

// for ENOMEM
#include <errno.h>

#include "lrmalloc.h"
#include "size_classes.h"
#include "pages.h"
#include "pagemap.h"
#include "log.h"
#include "tcache.h"

// global variables
// descriptor recycle list
std::atomic<DescriptorNode> AvailDesc({ nullptr });
// malloc init state
bool MallocInit = false;
// heaps, one heap per size class
ProcHeap Heaps[MAX_SZ_IDX];

// (un)register descriptor pages with pagemap
// all pages used by the descriptor will point to desc in
//  the pagemap
// for (unaligned) large allocations, only first page points to desc
// aligned large allocations get the corresponding page pointing to desc
void UpdatePageMap(ProcHeap* heap, char* ptr, Descriptor* desc, size_t scIdx)
{
    ASSERT(ptr);

    PageInfo info;
    info.Set(desc, scIdx);

    // large allocation, don't need to (un)register every page
    // just first
    if (!heap)
    {
        sPageMap.SetPageInfo(ptr, info);
        return;
    }

    // only need to worry about alignment for large allocations
    // ASSERT(ptr == superblock);

    // small allocation, (un)register every page
    // could *technically* optimize if blockSize >>> page, 
    //  but let's not worry about that
    size_t sbSize = heap->GetSizeClass()->sbSize;
    // sbSize is a multiple of page
    ASSERT((sbSize & PAGE_MASK) == 0);
    for (size_t idx = 0; idx < sbSize; idx += PAGE)
        sPageMap.SetPageInfo(ptr + idx, info); 
}

void RegisterDesc(Descriptor* desc)
{
    ProcHeap* heap = desc->heap;
    char* ptr = desc->superblock;
    size_t scIdx = 0;
    if (LIKELY(heap != nullptr))
        scIdx = heap->scIdx;

    UpdatePageMap(heap, ptr, desc, scIdx);
}

// unregister descriptor before superblock deletion
// can only be done when superblock is about to be free'd to OS
void UnregisterDesc(ProcHeap* heap, char* superblock)
{
    UpdatePageMap(heap, superblock, nullptr, 0L);
}

LFMALLOC_INLINE
PageInfo GetPageInfoForPtr(void* ptr)
{
    return sPageMap.GetPageInfo((char*)ptr);
}

// compute block index in superblock
LFMALLOC_INLINE
uint32_t ComputeIdx(char* superblock, char* block, size_t scIdx)
{
    SizeClassData* sc = &SizeClasses[scIdx];
    uint32_t scBlockSize = sc->blockSize;
    (void)scBlockSize; // suppress unused var warning

    ASSERT(block >= superblock);
    ASSERT(block < superblock + sc->sbSize);
    // optimize integer division by allowing the compiler to create 
    //  a jump table using size class index
    // compiler can then optimize integer div due to known divisor
    uint32_t diff = uint32_t(block - superblock);
    uint32_t idx = 0;
    switch (scIdx)
    {
#define SIZE_CLASS_bin_yes(index, blockSize)  \
        case index:                           \
            ASSERT(scBlockSize == blockSize); \
            idx = diff / blockSize;           \
            break;
#define SIZE_CLASS_bin_no(index, blockSize)
#define SC(index, lg_grp, lg_delta, ndelta, psz, bin, pgs, lg_delta_lookup) \
        SIZE_CLASS_bin_##bin((index + 1), ((1U << lg_grp) + (ndelta << lg_delta)))
        SIZE_CLASSES
        default:
            ASSERT(false);
            break;
    }
#undef SIZE_CLASS_bin_yes
#undef SIZE_CLASS_bin_no
#undef SC

    ASSERT(diff / scBlockSize == idx);
    return idx;
}

SizeClassData* ProcHeap::GetSizeClass() const
{
    return &SizeClasses[scIdx];
}

Descriptor* ListPopPartial(ProcHeap* heap)
{
    std::atomic<DescriptorNode>& list = heap->partialList;
    DescriptorNode oldHead = list.load();
    DescriptorNode newHead;
    do
    {
        Descriptor* oldDesc = oldHead.GetDesc();
        if (!oldDesc)
            return nullptr;

        newHead = oldDesc->nextPartial.load();
        Descriptor* desc = newHead.GetDesc();
        uint64_t counter = oldHead.GetCounter();
        newHead.Set(desc, counter);
    }
    while (!list.compare_exchange_weak(oldHead, newHead));

    return oldHead.GetDesc();
}

void ListPushPartial(Descriptor* desc)
{
    ProcHeap* heap = desc->heap;
    std::atomic<DescriptorNode>& list = heap->partialList;

    DescriptorNode oldHead = list.load();
    DescriptorNode newHead;
    do
    {
        newHead.Set(desc, oldHead.GetCounter() + 1);
        ASSERT(oldHead.GetDesc() != newHead.GetDesc());
        newHead.GetDesc()->nextPartial.store(oldHead); 
    }
    while (!list.compare_exchange_weak(oldHead, newHead));
}

void HeapPushPartial(Descriptor* desc)
{
    ListPushPartial(desc);
}

Descriptor* HeapPopPartial(ProcHeap* heap)
{
    return ListPopPartial(heap);
}

void MallocFromPartial(size_t scIdx, TCacheBin* cache, size_t& blockNum)
{
    ProcHeap* heap = &Heaps[scIdx];

    Descriptor* desc = HeapPopPartial(heap);
    if (!desc)
        return;

    // reserve block(s)
    Anchor oldAnchor = desc->anchor.load();
    Anchor newAnchor;
    uint32_t maxcount = desc->maxcount;
    uint32_t blockSize = desc->blockSize;
    char* superblock = desc->superblock;

    // we have "ownership" of block, but anchor can still change
    // due to free()
    do
    {
        if (oldAnchor.state == SB_EMPTY)
        {
            DescRetire(desc);
            // retry
            return MallocFromPartial(scIdx, cache, blockNum);
        }

        // oldAnchor must be SB_PARTIAL
        // can't be SB_FULL because we *own* the block now
        // and it came from HeapPopPartial
        // can't be SB_EMPTY, we already checked
        // obviously can't be SB_ACTIVE
        ASSERT(oldAnchor.state == SB_PARTIAL);

        newAnchor = oldAnchor;
        newAnchor.count = 0;
        // avail value doesn't actually matter
        newAnchor.avail = maxcount;
        newAnchor.state = SB_FULL;
    }
    while (!desc->anchor.compare_exchange_weak(
                oldAnchor, newAnchor));

    // will take as many blocks as available from superblock
    // *AND* no thread can do malloc() using this superblock, we
    //  exclusively own it
    // if CAS fails, it just means another thread added more available blocks
    //  through FlushCache, which we can then use
    uint32_t blocksTaken = oldAnchor.count;
    uint32_t avail = oldAnchor.avail;

    ASSERT(avail < maxcount);
    char* block = superblock + avail * blockSize;

    // cache must be empty at this point
    // and the blocks are already organized as a list
    // so all we need do is "push" that list, a constant time op
    ASSERT(cache->GetBlockNum() == 0);
    cache->PushList(block, blocksTaken);

    blockNum += blocksTaken;
}

void MallocFromNewSB(size_t scIdx, TCacheBin* cache, size_t& blockNum)
{
    ProcHeap* heap = &Heaps[scIdx];
    SizeClassData* sc = &SizeClasses[scIdx];

    Descriptor* desc = DescAlloc();
    ASSERT(desc);

    uint32_t const blockSize = sc->blockSize;
    uint32_t const maxcount = sc->GetBlockNum();

    desc->heap = heap;
    desc->blockSize = blockSize;
    desc->maxcount = maxcount;
    desc->superblock = (char*)PageAlloc(sc->sbSize);

    // prepare block list
    char* superblock = desc->superblock;
    for (uint32_t idx = 0; idx < maxcount - 1; ++idx)
    {
        char* block = superblock + idx * blockSize;
        char* next = superblock + (idx + 1) * blockSize;
        *(char**)block = next;
    }

    // push blocks to cache
    char* block = superblock; // first block
    cache->PushList(block, maxcount);

    Anchor anchor;
    anchor.avail = maxcount;
    anchor.count = 0;
    anchor.state = SB_FULL;

    desc->anchor.store(anchor);

    ASSERT(anchor.avail < maxcount || anchor.state == SB_FULL);
    ASSERT(anchor.count < maxcount);

    // register new descriptor
    // must be done before setting superblock as active
    // or leaving superblock as available in a partial list
    RegisterDesc(desc);

    // if state changes to SB_PARTIAL, desc must be added to partial list
    ASSERT(anchor.state == SB_FULL);

    blockNum += maxcount;
}

Descriptor* DescAlloc()
{
    DescriptorNode oldHead = AvailDesc.load();
    while (true)
    {
        Descriptor* desc = oldHead.GetDesc();
        if (desc)
        {
            DescriptorNode newHead = desc->nextFree.load();
            newHead.Set(newHead.GetDesc(), oldHead.GetCounter());
            if (AvailDesc.compare_exchange_weak(oldHead, newHead))
            {
                ASSERT(desc->blockSize == 0);
                return desc;
            }
        }
        else
        {
            // allocate several pages
            // get first descriptor, this is returned to caller
            char* ptr = (char*)PageAlloc(DESCRIPTOR_BLOCK_SZ);
            Descriptor* ret = (Descriptor*)ptr;
            // organize list with the rest of descriptors
            // and add to available descriptors
            {
                Descriptor* first = nullptr;
                Descriptor* prev = nullptr;

                char* currPtr = ptr + sizeof(Descriptor);
                currPtr = ALIGN_ADDR(currPtr, CACHELINE);
                first = (Descriptor*)currPtr;
                while (currPtr + sizeof(Descriptor) <
                        ptr + DESCRIPTOR_BLOCK_SZ)
                {
                    Descriptor* curr = (Descriptor*)currPtr;
                    if (prev)
                        prev->nextFree.store({ curr });

                    prev = curr;
                    currPtr = currPtr + sizeof(Descriptor);
                    currPtr = ALIGN_ADDR(currPtr, CACHELINE);
                }

                prev->nextFree.store({ nullptr });

                // add list to available descriptors
                DescriptorNode oldHead = AvailDesc.load();
                DescriptorNode newHead;
                do
                {
                    prev->nextFree.store(oldHead);
                    newHead.Set(first, oldHead.GetCounter() + 1);
                }
                while (!AvailDesc.compare_exchange_weak(oldHead, newHead));
            }

            return ret;
        }
    }
}

void DescRetire(Descriptor* desc)
{
    desc->blockSize = 0;
    DescriptorNode oldHead = AvailDesc.load();
    DescriptorNode newHead;
    do
    {
        desc->nextFree.store(oldHead);

        newHead.Set(desc, oldHead.GetCounter() + 1);
    }
    while (!AvailDesc.compare_exchange_weak(oldHead, newHead));
}

void FillCache(size_t scIdx, TCacheBin* cache)
{
    // at most cache will be filled with number of blocks equal to superblock
    size_t blockNum = 0;
    // use a *SINGLE* partial superblock to try to fill cache
    MallocFromPartial(scIdx, cache, blockNum);
    // if we obtain no blocks from partial superblocks, create a new superblock
    if (blockNum == 0)
        MallocFromNewSB(scIdx, cache, blockNum);

    SizeClassData* sc = &SizeClasses[scIdx];
    (void)sc;
    ASSERT(blockNum > 0);
    ASSERT(blockNum <= sc->cacheBlockNum);
}

void FlushCache(size_t scIdx, TCacheBin* cache)
{
    ProcHeap* heap = &Heaps[scIdx];
    SizeClassData* sc = &SizeClasses[scIdx];
    uint32_t const sbSize = sc->sbSize;
    uint32_t const blockSize = sc->blockSize;
    // after CAS, desc might become empty and
    //  concurrently reused, so store maxcount
    uint32_t const maxcount = sc->GetBlockNum();
    (void)maxcount; // suppress unused warning

    // @todo: optimize
    // in the normal case, we should be able to return several
    //  blocks with a single CAS
    while (cache->GetBlockNum() > 0)
    {
        char* head = cache->PeekBlock();
        char* tail = head;
        PageInfo info = GetPageInfoForPtr(head);
        Descriptor* desc = info.GetDesc();
        char* superblock = desc->superblock;

        // cache is a linked list of blocks
        // superblock free list is also a linked list of blocks
        // can optimize transfers of blocks between these 2 entities
        //  by exploiting existing structure
        uint32_t blockCount = 1;
        // check if next cache blocks are in the same superblock
        // same superblock, same descriptor
        while (cache->GetBlockNum() > blockCount)
        {
            char* ptr = *(char**)tail;
            if (ptr < superblock || ptr >= superblock + sbSize)
                break; // ptr not in superblock

            // ptr in superblock, add to "list"
            ++blockCount;
            tail = ptr;
        }

        cache->PopList(*(char**)tail, blockCount);

        // add list to desc, update anchor
        uint32_t idx = ComputeIdx(superblock, head, scIdx);

        Anchor oldAnchor = desc->anchor.load();
        Anchor newAnchor;
        do
        {
            // update anchor.avail
            char* next = (char*)(superblock + oldAnchor.avail * blockSize);
            *(char**)tail = next;

            newAnchor = oldAnchor;
            newAnchor.avail = idx;
            // state updates
            // don't set SB_PARTIAL if state == SB_ACTIVE
            if (oldAnchor.state == SB_FULL)
                newAnchor.state = SB_PARTIAL;
            // this can't happen with SB_ACTIVE
            // because of reserved blocks
            ASSERT(oldAnchor.count < desc->maxcount);
            if (oldAnchor.count + blockCount == desc->maxcount)
            {
                newAnchor.count = desc->maxcount - 1;
                newAnchor.state = SB_EMPTY; // can free superblock
            }
            else
                newAnchor.count += blockCount;
        }
        while (!desc->anchor.compare_exchange_weak(
                    oldAnchor, newAnchor));

        // after last CAS, can't reliably read any desc fields
        // as desc might have become empty and been concurrently reused
        ASSERT(oldAnchor.avail < maxcount || oldAnchor.state == SB_FULL);
        ASSERT(newAnchor.avail < maxcount);
        ASSERT(newAnchor.count < maxcount);

        // CAS success, can free block
        if (newAnchor.state == SB_EMPTY)
        {
            // unregister descriptor
            UnregisterDesc(heap, superblock);

            // free superblock
            PageFree(superblock, heap->GetSizeClass()->sbSize);
        }
        else if (oldAnchor.state == SB_FULL)
            HeapPushPartial(desc);
    }
}

void InitMalloc()
{
    LOG_DEBUG();

    // hard assumption that this can't be called concurrently
    MallocInit = true;

    // init size classes
    InitSizeClass();

    // init page map
    sPageMap.Init();

    // init heaps
    for (size_t idx = 0; idx < MAX_SZ_IDX; ++idx)
    {
        ProcHeap& heap = Heaps[idx];
        heap.partialList.store({ nullptr });
        heap.scIdx = idx;
    }
}

LFMALLOC_INLINE
void* do_malloc(size_t size)
{
    // ensure malloc is initialized
    if (UNLIKELY(!MallocInit))
        InitMalloc();

    // large block allocation
    if (UNLIKELY(size > MAX_SZ))
    {
        size_t pages = PAGE_CEILING(size);
        Descriptor* desc = DescAlloc();
        ASSERT(desc);

        desc->heap = nullptr;
        desc->blockSize = pages;
        desc->maxcount = 1;
        desc->superblock = (char*)PageAlloc(pages);

        Anchor anchor;
        anchor.avail = 0;
        anchor.count = 0;
        anchor.state = SB_FULL;

        desc->anchor.store(anchor);

        RegisterDesc(desc);

        char* ptr = desc->superblock;
        LOG_DEBUG("large, ptr: %p", ptr);
        return (void*)ptr;
    }

    // size class calculation
    size_t scIdx = GetSizeClass(size);

    TCacheBin* cache = &TCache[scIdx];
    // fill cache if needed
    if (UNLIKELY(cache->GetBlockNum() == 0))
        FillCache(scIdx, cache);

    return cache->PopBlock();
}

LFMALLOC_INLINE
bool isPowerOfTwo(size_t x)
{
    // https://stackoverflow.com/questions/3638431/determine-if-an-int-is-a-power-of-2-or-not-in-a-single-line
    return x && !(x & (x - 1));
}

LFMALLOC_INLINE
void* do_aligned_alloc(size_t alignment, size_t size)
{
    if (UNLIKELY(!isPowerOfTwo(alignment)))
        return nullptr;

    size = ALIGN_VAL(size, alignment);

    ASSERT(size > 0 && alignment > 0 && size >= alignment);

    // @todo: almost equal logic to do_malloc, DRY
    // ensure malloc is initialized
    if (UNLIKELY(!MallocInit))
        InitMalloc();

    // allocations smaller than PAGE will be correctly aligned
    // this is because size >= alignment, and size will map to a small class size
    //  with the formula 2^X + A*2^(X-1) + C*2^(X-2)
    // since size is a multiple of alignment, the lowest size class power of two
    //  is already >= alignment
    // this does not work if allocation > PAGE even if it's a small class size,
    //  because superblock for those allocations is only guaranteed
    //  to be page aligned
    // force such allocations to become large block allocs
    if (UNLIKELY(size > PAGE))
    {
        // hotfix solution for this case is to force allocation to be large
        size = std::max<size_t>(size, MAX_SZ + 1);

        // large blocks are page-aligned
        // if user asks for a diabolical alignment, need more pages to fulfil it
        bool const needsMorePages = (alignment > PAGE);
        if (UNLIKELY(needsMorePages))
            size += alignment;

        size_t pages = PAGE_CEILING(size);
        Descriptor* desc = DescAlloc();
        ASSERT(desc);

        char* ptr = (char*)PageAlloc(pages);

        desc->heap = nullptr;
        desc->blockSize = pages;
        desc->maxcount = 1;
        desc->superblock = ptr;

        Anchor anchor;
        anchor.avail = 0;
        anchor.count = 0;
        anchor.state = SB_FULL;

        desc->anchor.store(anchor);

        RegisterDesc(desc);

        if (UNLIKELY(needsMorePages))
        {
            ptr = ALIGN_ADDR(ptr, alignment);
            // aligned block must fit into allocated pages
            ASSERT((ptr + size) <= (desc->superblock + desc->blockSize));

            // need to update page so that descriptors can be found
            //  for large allocations aligned to "middle" of superblocks
            UpdatePageMap(nullptr, ptr, desc, 0L);
        }

        LOG_DEBUG("large, ptr: %p", ptr);
        return (void*)ptr;
    }

    ASSERT(size <= PAGE);

    // size class calculation
    size_t scIdx = GetSizeClass(size);

    TCacheBin* cache = &TCache[scIdx];
    // fill cache if needed
    if (UNLIKELY(cache->GetBlockNum() == 0))
        FillCache(scIdx, cache);

    return cache->PopBlock();
}

LFMALLOC_INLINE
void do_free(void* ptr)
{
    PageInfo info = GetPageInfoForPtr(ptr);
    Descriptor* desc = info.GetDesc();
    // @todo: this can happen with dynamic loading
    // need to print correct message
    ASSERT(desc);

    size_t scIdx = info.GetScIdx();
 
    LOG_DEBUG("Heap %p, Desc %p, ptr %p", heap, desc, ptr);

    // large allocation case
    if (UNLIKELY(!scIdx))
    {
        char* superblock = desc->superblock;

        // unregister descriptor
        UnregisterDesc(nullptr, superblock);
        // aligned large allocation case
        if (UNLIKELY((char*)ptr != superblock))
            UnregisterDesc(nullptr, (char*)ptr);

        // free superblock
        PageFree(superblock, desc->blockSize);

        // desc cannot be in any partial list, so it can be
        //  immediately reused
        DescRetire(desc);
        return;
    }

    TCacheBin* cache = &TCache[scIdx];
    SizeClassData* sc = &SizeClasses[scIdx];

    // flush cache if need
    if (UNLIKELY(cache->GetBlockNum() >= sc->cacheBlockNum))
        FlushCache(scIdx, cache);

    cache->PushBlock((char*)ptr);
}

void lf_malloc_initialize() { }

void lf_malloc_finalize() { }

void lf_malloc_thread_initialize() { }

void lf_malloc_thread_finalize()
{
    // flush caches
    for (size_t scIdx = 1; scIdx < MAX_SZ_IDX; ++scIdx)
        FlushCache(scIdx, &TCache[scIdx]);
}

extern "C"
void* lf_malloc(size_t size) noexcept
{
    LOG_DEBUG("size: %lu", size);

    return do_malloc(size);
}

extern "C"
void* lf_calloc(size_t n, size_t size) noexcept
{
    LOG_DEBUG();
    size_t allocSize = n * size;
    // overflow check
    // @todo: expensive, need to optimize
    if (UNLIKELY(n == 0 || allocSize / n != size))
        return nullptr;

    void* ptr = do_malloc(allocSize);

    // calloc returns zero-filled memory
    // @todo: optimize, memory may be already zero-filled 
    //  if coming directly from OS
    if (LIKELY(ptr != nullptr))
        memset(ptr, 0x0, allocSize);

    return ptr;
}

extern "C"
void* lf_realloc(void* ptr, size_t size) noexcept
{
    LOG_DEBUG();

    size_t blockSize = 0;
    if (LIKELY(ptr != nullptr))
    {
        PageInfo info = GetPageInfoForPtr(ptr);
        Descriptor* desc = info.GetDesc();
        ASSERT(desc);

        blockSize = desc->blockSize;

        // realloc with size == 0 is the same as free(ptr)
        if (UNLIKELY(size == 0))
        {
            do_free(ptr);
            return nullptr;
        }

        // nothing to do, block is already large enough
        if (UNLIKELY(size <= blockSize))
            return ptr;
    }

    void* newPtr = do_malloc(size);
    if (LIKELY(ptr && newPtr))
    {
        memcpy(newPtr, ptr, blockSize);
        do_free(ptr);
    }

    return newPtr;
}

extern "C"
size_t lf_malloc_usable_size(void* ptr) noexcept
{
    LOG_DEBUG();
    if (UNLIKELY(ptr == nullptr))
        return 0;

    PageInfo info = GetPageInfoForPtr(ptr);

    size_t scIdx = info.GetScIdx();
    // large allocation case
    if (UNLIKELY(!scIdx))
    {
        Descriptor* desc = info.GetDesc();
        ASSERT(desc);
        return desc->blockSize;
    }

    SizeClassData* sc = &SizeClasses[scIdx];
    return sc->blockSize;
}

extern "C"
int lf_posix_memalign(void** memptr, size_t alignment, size_t size) noexcept
{
    LOG_DEBUG();

    // "EINVAL - The alignment argument was not a power of two, or
    //  was not a multiple of sizeof(void *)"
    if (UNLIKELY(!isPowerOfTwo(alignment) || (alignment & PTR_MASK)))
        return EINVAL;

    void* ptr = do_aligned_alloc(alignment, size);
    if (UNLIKELY(ptr == nullptr))
        return ENOMEM;

    ASSERT(memptr);
    *memptr = ptr; 
    return 0;
}

extern "C"
void* lf_aligned_alloc(size_t alignment, size_t size) noexcept
{
    LOG_DEBUG();
    return do_aligned_alloc(alignment, size);
}

extern "C"
void* lf_valloc(size_t size) noexcept
{
    LOG_DEBUG();
    return do_aligned_alloc(PAGE, size);
}

extern "C"
void* lf_memalign(size_t alignment, size_t size) noexcept
{
    LOG_DEBUG();
    return do_aligned_alloc(alignment, size);
}

extern "C"
void* lf_pvalloc(size_t size) noexcept
{
    LOG_DEBUG();
    return do_aligned_alloc(PAGE, size);
}

extern "C"
void lf_free(void* ptr) noexcept
{
    LOG_DEBUG("ptr: %p", ptr);
    if (UNLIKELY(!ptr))
        return;

    do_free(ptr);
}



