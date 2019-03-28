#include "PageMap.hpp"

#include "RegionManager.hpp"
inline void PageInfo::Set(Descriptor* desc, size_t scIdx)
{
	assert(((size_t)desc & SC_MASK) == 0);
	assert(scIdx < MAX_SZ_IDX);

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

PageMap::PageMap(){
	std::string path = HEAPFILE_PREFIX + std::string("_pagemap");
	bool persist = true;
	if(RegionManager::exists_test(path)){
		mgr = new RegionManager(path,persist,PM_SZ);
		void* hstart = mgr->__fetch_heap_start();
		_pagemap = (std::atomic<PageInfo>*) hstart;
		//todo: do we need recovery?
	} else {
		//doesn't exist. create a new one
		mgr = new RegionManager(path,persist,PM_SZ);
		bool res = mgr->__nvm_region_allocator((void**)&_pagemap,PAGESIZE,PM_SZ);
		if(!res) assert(0&&"mgr allocation fails!");
		mgr->__store_heap_start(_pagemap);
	}
}

PageMap::~PageMap(){
	FLUSHFENCE;
	delete mgr;
}

inline size_t PageMap::AddrToKey(char* ptr) const
{
	size_t key = ((size_t)ptr >> PM_KEY_SHIFT) & PM_KEY_MASK;
	return key;
}

inline PageInfo PageMap::GetPageInfo(char* ptr)
{
	size_t key = AddrToKey(ptr);
	PageInfo ret = _pagemap[key].load();
	FLUSH(&_pagemap[key]);
	FLUSHFENCE;
	return ret;
}

inline void PageMap::SetPageInfo(char* ptr, PageInfo info)
{
	size_t key = AddrToKey(ptr);
	FLUSHFENCE;
	_pagemap[key].store(info);
	FLUSH(&_pagemap[key]);
}