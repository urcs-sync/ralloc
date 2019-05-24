#include "PageMap.hpp"

#include "RegionManager.hpp"
using namespace pmmalloc;
PageMap pmmalloc::pagemap;
void PageInfo::set(Descriptor* desc, size_t sc_idx)
{
	assert(pmmalloc::initialized&&"PM should be initialized first.");
	assert(pmmalloc::mgr&&"mgr should not be NULL.");
	assert(((size_t)desc & SC_MASK) == 0);
	assert(sc_idx < MAX_SZ_IDX);

	_desc = ((size_t)desc | sc_idx) - (uint64_t)pmmalloc::mgr->base_addr;
}

Descriptor* PageInfo::get_desc() const
{
	assert(pmmalloc::initialized&&"PM should be initialized first.");
	assert(pmmalloc::mgr&&"mgr should not be NULL.");
	return (Descriptor*)((_desc + (uint64_t)pmmalloc::mgr->base_addr) & ~SC_MASK);
}

size_t PageInfo::get_sc_idx() const
{
	assert(pmmalloc::initialized&&"PM should be initialized first.");
	assert(pmmalloc::mgr&&"mgr should not be NULL.");
	return ((_desc + (uint64_t)pmmalloc::mgr->base_addr) & SC_MASK);
}

PageMap::PageMap(){
	std::string path = HEAPFILE_PREFIX + std::string("_pagemap");
	bool persist = false;
	if(RegionManager::exists_test(path)){
		page_mgr = new RegionManager(path,persist,PM_SZ);
		void* hstart = page_mgr->__fetch_heap_start();
		_pagemap = (std::atomic<PageInfo>*) hstart;
		//todo: do we need recovery?
	} else {
		//doesn't exist. create a new one
		page_mgr = new RegionManager(path,persist,PM_SZ);
		bool res = page_mgr->__nvm_region_allocator((void**)&_pagemap,PAGESIZE,PM_SZ);
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
	size_t key = ((size_t)ptr >> PM_KEY_SHIFT) & PM_KEY_MASK;
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
