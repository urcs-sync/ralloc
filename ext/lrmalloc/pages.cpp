/*
 * Copyright (C) 2019 Ricardo Leite. All rights reserved.
 * Licenced under the MIT licence. See COPYING file in the project root for details.
 */

#include <sys/mman.h>

#include "pages.h"
#include "log.h"

void* PageAlloc(size_t size)
{
    ASSERT((size & PAGE_MASK) == 0);

    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANON, -1, 0);
    if (ptr == MAP_FAILED)
        ptr = nullptr;
 
    return ptr;
}

void* PageAllocOvercommit(size_t size)
{
    ASSERT((size & PAGE_MASK) == 0);

    // use no MAP_NORESERVE to skip OS overcommit limits
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);
    if (ptr == MAP_FAILED)
        ptr = nullptr;
 
    return ptr;
}

void PageFree(void* ptr, size_t size)
{
    ASSERT((size & PAGE_MASK) == 0);

    int ret = munmap(ptr, size);
    (void)ret; // suppress warning
    ASSERT(ret == 0);
}

