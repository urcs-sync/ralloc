#include "TCache.hpp"

thread_local TCacheBin pmmalloc::t_cache[MAX_SZ_IDX];