/*
 * (c) Copyright 2016 Hewlett Packard Enterprise Development LP
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version. This program is
 * distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details. You should have received a copy of the GNU Lesser
 * General Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */

/* 
 * This is the C++ version of region_manager code from Makalu
 * https://github.com/HewlettPackard/Atlas/tree/makalu/makalu_alloc
 *
 * I did some modification including making __nvm_region_allocator() 
 * to be nonblocking.
 */

#ifndef _REGION_MANAGER_HPP_
#define _REGION_MANAGER_HPP_

#include <string>
#include <fstream>
#include <atomic>
#include <vector>

#include "pm_config.hpp"
#include "pfence_util.h"
#include "pptr.hpp"


/* layout of the region:
 *	(The first page (4K) is reserved)
 *	atomic_pptr<char> curr_addr  0~63 (base_addr points to)
 *	heap_start = root - base_start 64~127
 *	uint64_t size 128~191
 *	...
 *	(the first page ends and heap starts here to which heap_start points)
 *	....
 *	(heap ends here to which curr_addr points)
 */
class RegionManager{
public:
	const uint64_t FILESIZE;
	const std::string HEAPFILE;
	int FD = 0;
	char *base_addr = nullptr;
	atomic_pptr<char>* curr_addr_ptr;//this always points to the place of base_addr
	bool persist;

	RegionManager(const std::string& file_path, uint64_t size, bool p = true, bool imm_expand = true):
		FILESIZE(((size/PAGESIZE)+2)*PAGESIZE), // size should align to page
		HEAPFILE(file_path),
		curr_addr_ptr(nullptr),
		persist(p){
		assert(size%CACHELINE_SIZE == 0); // size should be multiple of cache line size
		if(persist){
			if(exists_test(HEAPFILE)){
				__remap_persistent_region();
			} else {
				__map_persistent_region();
				if(imm_expand){//expand immediately
					void* t;
					bool res = __nvm_region_allocator(&t,CACHELINE_SIZE,size); 
					if(!res) assert(0&&"region allocation fails!");
					__store_heap_start(t);
				}
			}
		} else {
			if(exists_test(HEAPFILE)){
				__remap_transient_region();
			} else {
				__map_transient_region();
				if(imm_expand){//expand immediately
					void* t;
					bool res = __nvm_region_allocator(&t,CACHELINE_SIZE,size); 
					if(!res) assert(0&&"region allocation fails!");
					__store_heap_start(t);
				}
			}
		}
	};
	~RegionManager(){
		if(persist)
			__close_persistent_region();
		else
			__close_transient_region();
#ifdef DESTROY
		__destroy();
#endif
	}
	//mmap anynomous, not used by default
	// void __map_transient_region();

	inline static bool exists_test (const std::string& name){
		std::ifstream f(name.c_str());
		return f.good();
	}

	//mmap file
	//the only difference between persist and trans version is
	//persist always map to the same addr while trans doesn't
	void __map_persistent_region();
	void __remap_persistent_region();
	void __map_transient_region();
	void __remap_transient_region();

	//persist the curr and base address
	void __close_persistent_region();

	//flush transient region back
	void __close_transient_region();

	//store heap root by offset from base
	void __store_heap_start(void*);

	//retrieve heap root from offset from base
	void* __fetch_heap_start();

	/* return true if succeeds, otherwise false
	 * ret should be flushed after the call as a return value if needed
	 */
	bool __nvm_region_allocator(void** /*ret */, size_t /* alignment */, size_t /*size */);

	/* try to expand the region.
	 *  0: succeed
	 * +1: fail due to completed expansion during the routine
	 * -1: fail due to wrong parameter or out of space error
	 */
	int __try_nvm_region_allocator(void** /*ret */, size_t /* alignment */, size_t /*size */);

	//true if ptr is in persistent region, otherwise false
	bool __within_range(void* ptr);

	//destroy the region and delete the file
	void __destroy();
};


class Regions{
public:
	std::vector<RegionManager*> regions;
	std::vector<char*> regions_address; // base address of each region
	Regions():
		regions(),
		regions_address(){
			regions_address.reserve(8);// a cache line
		};
	~Regions(){
		for(auto it:regions) delete(it);
		regions.clear();
		regions_address.clear();
	}

	inline static bool exists_test (const std::string& name){
		std::ifstream f(name.c_str());
		return f.good();
	}

	void destroy(){
		for(auto it:regions) delete(it);
		regions.clear();
		regions_address.clear();
	}

	void create(const std::string& file_path, uint64_t size, bool p = true, bool imm_expand = true){
		assert(regions.size() == regions_address.size());
		bool restart = exists_test(file_path);
		RegionManager* new_mgr = new RegionManager(file_path,size,p,imm_expand);
		regions.push_back(new_mgr);
		if(imm_expand || restart)
			regions_address.push_back((char*)new_mgr->__fetch_heap_start());
		else
			regions_address.push_back(nullptr);
		return;
	}

	template<class T>
	T* create_for(const std::string& file_path, uint64_t size, bool p = true){
		assert(regions.size() == regions_address.size());

		bool restart = exists_test(file_path);
		RegionManager* new_mgr = new RegionManager(file_path,size,p,true);
		regions.push_back(new_mgr);
		T* t = (T*) new_mgr->__fetch_heap_start();
		if(!restart){
			new (t) T();
		}
		regions_address.push_back((char*)t);
		return t;
	}

	// caller should ensure regions_address is created.
	inline char* lookup(int index) const{
		return regions_address[index];
	}

	// caller should ensure regions_address is created.
	inline char* translate(int index, char* relative_address) const{
		return regions_address[index] + (size_t)relative_address;
	}

	// caller should ensure regions_address is created.
	inline char* untranslate(int index, char* absolute_address) const{
		return (char*)((uint64_t)absolute_address - (uint64_t)regions_address[index]);
	}

	inline bool expand(int index, void** memptr, size_t alignment, size_t size){
		void* tmp;
		if(memptr == nullptr) memptr = &tmp;
		return regions[index]->__nvm_region_allocator(memptr, alignment, size);
	}

	inline bool in_range(int index, const void* ptr){
		bool ret = ptr >= regions_address[index];
		if (!ret) return false;
		return ret && (ptr < regions[index]->curr_addr_ptr->load());
	}

	inline void flush_region(int index){
		RegionManager* target = regions[index];
		char* addr = regions_address[index];
		char* ending = target->curr_addr_ptr->load();
		for(; addr < ending; addr += CACHELINE_SIZE) {
			FLUSH(addr);
		}
		FLUSHFENCE;
	}
};

#endif /* _REGION_MANAGER_HPP_ */