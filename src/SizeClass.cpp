/*
 * Copyright (C) 2019 University of Rochester. All rights reserved.
 * Licenced under the MIT licence. See LICENSE file in the project root for
 * details. 
 */

/*
 * Copyright (C) 2018 Ricardo Leite
 * Licenced under the MIT licence. This file shares some portion from
 * LRMalloc(https://github.com/ricleite/lrmalloc) and its copyright 
 * is retained. See LICENSE for details about MIT License.
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


