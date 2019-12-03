/*

Copyright 2019 University of Rochester

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. 

*/ 

#include "pm_config.hpp"
#include "SizeClass.hpp"

// here we use same size for sbs in different sizeclass for easy management
#define SIZE_CLASS_bin_yes(block_size, pages) \
	{ block_size, SBSIZE, SBSIZE/block_size, SBSIZE/block_size },
/* #define SIZE_CLASS_bin_yes(block_size, pages) \
 	{ block_size, pages * PAGESIZE, 0, 0 },
 	*/
#define SIZE_CLASS_bin_no(block_size, pages)

#define SC(index, lg_grp, lg_delta, ndelta, psz, bin, pgs, lg_delta_lookup) \
	SIZE_CLASS_bin_##bin(((1U << lg_grp) + (ndelta << lg_delta)), pgs)

// this is reconstructed in every execution
SizeClass ralloc::sizeclass;

SizeClass::SizeClass():
	sizeclasses{
		{ 0, 0, 0, 0},
		SIZE_CLASSES
	},
	sizeclass_lookup{0} {
	// first size class reserved for large allocations
	size_t lookupIdx = 0;
	for (size_t sc_idx = 1; sc_idx < MAX_SZ_IDX; ++sc_idx)
	{
		SizeClassData const& sc = sizeclasses[sc_idx];
		size_t block_size = sc.block_size;
		while (lookupIdx <= block_size)
		{
			sizeclass_lookup[lookupIdx] = sc_idx;
			++lookupIdx;
		} 
	}
}


