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

#include "pm_config.hpp"

class RegionManager{
	const uint64_t FILESIZE;
	const std::string HEAPFILE;
public:
	int FD = 0;
	char *base_addr = nullptr;
	std::atomic<char *>* curr_addr_ptr;//this always points to the place of base_addr+1
	bool persist;

	RegionManager(const std::string& file_path, bool p = true, uint64_t size = MAX_FILESIZE):
		FILESIZE(size+PAGESIZE),
		HEAPFILE(file_path),
		curr_addr_ptr(nullptr),
		persist(p){
		if(persist){
			if(exists_test(HEAPFILE)){
				__remap_persistent_region();
			} else {
				__map_persistent_region();
			}
		} else {
			if(exists_test(HEAPFILE)){
				__remap_transient_region();
			} else {
				__map_transient_region();
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


#endif /* _REGION_MANAGER_HPP_ */