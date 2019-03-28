
#include "pm_config.hpp"
#include "SizeClass.hpp"

#define SIZE_CLASS_bin_yes(block_size, pages) \
	{ block_size, pages * PAGESIZE, 0, 0 },
#define SIZE_CLASS_bin_no(block_size, pages)

#define SC(index, lg_grp, lg_delta, ndelta, psz, bin, pgs, lg_delta_lookup) \
	SIZE_CLASS_bin_##bin((1U << lg_grp) + (ndelta << lg_delta), pgs)


SizeClass::SizeClass():
	sizeclasses{
		{ 0, 0, 0, 0},
		SIZE_CLASSES
	},
	sizeclass_lookup{0} {
	// each superblock has to contain several blocks
	// and it has to contain blocks *perfectly*
	//  e.g no space left after last block
	for (size_t sc_idx = 1; sc_idx < MAX_SZ_IDX; ++sc_idx)
	{
		SizeClassData& sc = sizeclasses[sc_idx];
		size_t block_size = sc.block_size;
		size_t sb_size = sc.sb_size;
		// size class large enough to store several elements
		if (sb_size > block_size && (sb_size % block_size) == 0)
			continue; // skip

		// increase superblock size so it can hold >1 elements
		while (block_size >= sb_size)
			sb_size += sc.sb_size;

		sc.sb_size = sb_size;
	}

	// increase superblock size if need
	for (size_t sc_idx = 1; sc_idx < MAX_SZ_IDX; ++sc_idx)
	{
		SizeClassData& sc = sizeclasses[sc_idx];
		size_t sb_size = sc.sb_size;
		// 2MB
		while (sb_size < (PAGESIZE * PAGESIZE))
			sb_size += sc.sb_size;

		sc.sb_size = sb_size;
	}

	// fill block_num and cache_block_num
	for (size_t sc_idx = 1; sc_idx < MAX_SZ_IDX; ++sc_idx)
	{
		SizeClassData& sc = sizeclasses[sc_idx];
		// block_num calc
		sc.block_num = sc.sb_size / sc.block_size;
		// cache_block_num calc
		sc.cache_block_num = sc.block_num * 1;
		assert(sc.block_num > 0);
		assert(sc.block_num < MAX_BLOCK_NUM);
		assert(sc.block_num >= sc.cache_block_num);
	}

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


