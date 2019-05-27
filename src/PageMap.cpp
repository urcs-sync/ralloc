#include "PageMap.hpp"

#include "RegionManager.hpp"
using namespace rpmalloc;
PageMap rpmalloc::pagemap;
void PageInfo::set(Descriptor* desc, size_t sc_idx)
{
	assert(rpmalloc::initialized&&"PM should be initialized first.");
	assert(rpmalloc::mgr&&"mgr should not be NULL.");
	assert(((size_t)desc & SC_MASK) == 0);
	assert(sc_idx < MAX_SZ_IDX);

	_desc = desc==nullptr ? 0 : ((size_t)desc | sc_idx) - (uint64_t)rpmalloc::mgr->base_addr;
}

Descriptor* PageInfo::get_desc() const
{
	assert(rpmalloc::initialized&&"PM should be initialized first.");
	assert(rpmalloc::mgr&&"mgr should not be NULL.");
	return _desc==0 ? nullptr : (Descriptor*)((_desc + (uint64_t)rpmalloc::mgr->base_addr) & ~SC_MASK);
}

size_t PageInfo::get_sc_idx() const
{
	assert(rpmalloc::initialized&&"PM should be initialized first.");
	assert(rpmalloc::mgr&&"mgr should not be NULL.");
	return _desc==0 ? 0 : ((_desc + (uint64_t)rpmalloc::mgr->base_addr) & SC_MASK);
}

PageMap::PageMap(){
	std::string path = HEAPFILE_PREFIX + std::string("_pagemap");
	bool persist = false;
	if(RegionManager::exists_test(path)){
		page_mgr = new RegionManager(path,persist,RP_SZ);
		void* hstart = page_mgr->__fetch_heap_start();
		_pagemap = (std::atomic<PageInfo>*) hstart;
		//todo: do we need recovery?
	} else {
		//doesn't exist. create a new one
		page_mgr = new RegionManager(path,persist,RP_SZ);
		bool res = page_mgr->__nvm_region_allocator((void**)&_pagemap,PAGESIZE,RP_SZ);
		if(!res) assert(0&&"page_mgr allocation fails!");
		page_mgr->__store_heap_start(_pagemap);
	}
}

PageMap::~PageMap(){
	delete page_mgr;
}

size_t PageMap::addr_to_key(char* ptr) const
{
	assert(initialized&&"PM should be initialized first.");
	assert(rpmalloc::mgr&&"mgr should not be NULL.");
	uint64_t diff = (uint64_t)ptr - (uint64_t)rpmalloc::mgr->base_addr;
	size_t key = ((size_t)diff >> RP_KEY_SHIFT) & RP_KEY_MASK;
	return key;
}

PageInfo PageMap::get_page_info(char* ptr)
{
	assert(initialized&&"PM should be initialized first.");
	size_t key = addr_to_key(ptr);
	PageInfo ret = _pagemap[key].load();
	return ret;
}

void PageMap::set_page_info(char* ptr, PageInfo info)
{
	assert(initialized&&"PM should be initialized first.");
	size_t key = addr_to_key(ptr);
	_pagemap[key].store(info);
}
