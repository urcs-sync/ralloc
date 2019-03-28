/*
 * Copyright (C) 2007  Scott Schneider, Christos Antonopoulos
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <sys/mman.h>

#include <string>

#include "BaseMeta.hpp"

// some metadata from LRMalloc
__thread TCacheBin BaseMeta::TCache[MAX_SZ_IDX];

using namespace std;

BaseMeta::BaseMeta(RegionManager* m, uint64_t thd_num) : 
	mgr(m),
	thread_num(thd_num) {
	FLUSH(&thread_num);
	// free_desc = new ArrayQueue<Descriptor*>("pmmalloc_freedesc");
	free_sb = new ArrayStack<void*>("pmmalloc_freesb");
	/* allocate these persistent data into specific memory address */

	/* heaps init */
	for (size_t idx = 0; idx < MAX_SZ_IDX; ++idx){
		ProcHeap& heap = Heaps[idx];
		heap.partialList.store({ nullptr });
		heap.scIdx = idx;
		FLUSH(&Heaps[idx]);
	}

	/* persistent roots init */
	for(int i=0;i<MAX_ROOTS;i++){
		roots[i]=nullptr;
		FLUSH(&roots[i]);
	}

	FLUSHFENCE;
}

uint64_t BaseMeta::new_space(int i){//i=0:desc, i=1:small sb, i=2:large sb
	FLUSHFENCE;
	uint64_t my_space_num = space_num[i].fetch_add(1,std::memory_order_relaxed);
	if(my_space_num>=MAX_SECTION) assert(0&&"space number reaches max!");
	FLUSH(&space_num[i]);
	spaces[i][my_space_num].sec_bytes = 0;
	FLUSH(&spaces[i][my_space_num].sec_bytes);
	FLUSHFENCE;
	uint64_t space_size = i==0?DESC_SPACE_SIZE:SB_SPACE_SIZE;
	bool res = mgr->__nvm_region_allocator(&(spaces[i][my_space_num].sec_start),PAGESIZE, space_size);
	if(!res) assert(0&&"region allocation fails!");
	// spaces[i][my_space_num].sec_curr.store(spaces[i][my_space_num].sec_start);
	spaces[i][my_space_num].sec_bytes = space_size;
	FLUSH(&spaces[i][my_space_num].sec_start);
	// FLUSH(&spaces[i][my_space_num].sec_curr);
	FLUSH(&spaces[i][my_space_num].sec_bytes);
	FLUSHFENCE;
	return my_space_num;
}

inline size_t BaseMeta::get_sizeclass(size_t size){
	return sizeclass.GetSizeClass(size);
}

inline SizeClassData BaseMeta::get_sizeclass_by_idx(size_t idx) { 
	return sizeclass.GetSizeClassByIdx(idx);
}

// compute block index in superblock by addr to sb, block, and sc index
uint32_t BaseMeta::compute_idx(char* superblock, char* block, size_t sc_idx)
{
	SizeClassData sc = get_sizeclass_by_idx(sc_idx);
	uint32_t sc_block_size = sc.blockSize;
	(void)sc_block_size; // suppress unused var warning

	assert(block >= superblock);
	assert(block < superblock + sc.sbSize);
	// optimize integer division by allowing the compiler to create 
	//  a jump table using size class index
	// compiler can then optimize integer div due to known divisor
	uint32_t diff = uint32_t(block - superblock);
	uint32_t idx = 0;
	switch (sc_idx)
	{
#define SIZE_CLASS_bin_yes(index, block_size)		\
		case index:									\
			assert(sc_block_size == block_size);	\
			idx = diff / block_size;				\
			break;
#define SIZE_CLASS_bin_no(index, block_size)
#define SC(index, lg_grp, lg_delta, ndelta, psz, bin, pgs, lg_delta_lookup) \
		SIZE_CLASS_bin_##bin((index + 1), ((1U << lg_grp) + (ndelta << lg_delta)))
		SIZE_CLASSES
		default:
			assert(false);
			break;
	}
#undef SIZE_CLASS_bin_yes
#undef SIZE_CLASS_bin_no
#undef SC

	assert(diff / sc_block_size == idx);
	return idx;
}

void fill_cache(size_t sc_idx, TCacheBin* cache)
{
	// at most cache will be filled with number of blocks equal to superblock
	size_t blockNum = 0;
	// use a *SINGLE* partial superblock to try to fill cache
	malloc_from_partial(scIdx, cache, blockNum);
	// if we obtain no blocks from partial superblocks, create a new superblock
	if (blockNum == 0)
		malloc_from_newSB(scIdx, cache, blockNum);

	SizeClassData* sc = &SizeClasses[scIdx];
	(void)sc;
	ASSERT(blockNum > 0);
	ASSERT(blockNum <= sc->cacheBlockNum);
}








void* BaseMeta::small_sb_alloc(){
	void* sb = nullptr;
	if(auto tmp = free_sb->pop()){
		sb = tmp.value();
	}
	else{
		uint64_t space_num = new_space(1);
		cout<<"allocate sb space "<<space_num<<endl;
		sb = spaces[1][space_num].sec_start;
		organize_sb_list(sb,SB_SPACE_SIZE/SBSIZE,SBSIZE);
	}
	return sb;
}
void BaseMeta::small_sb_retire(void* sb){
	free_sb->push(sb);
}
//todo
void* BaseMeta::large_sb_alloc(size_t size, uint64_t alignement){
	cout<<"WARNING: Allocating a large object is not persisted yet!\n";
	// assert(0&"not persistently implemented yet!");
	void* addr = mmap(nullptr,size,PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (addr == MAP_FAILED) {
		fprintf(stderr, "large_sb_alloc() mmap failed, %lu: ", size);
		switch (errno) {
			case EBADF:	fprintf(stderr, "EBADF"); break;
			case EACCES:	fprintf(stderr, "EACCES"); break;
			case EINVAL:	fprintf(stderr, "EINVAL"); break;
			case ETXTBSY:	fprintf(stderr, "ETXBSY"); break;
			case EAGAIN:	fprintf(stderr, "EAGAIN"); break;
			case ENOMEM:	fprintf(stderr, "ENOMEM"); break;
			case ENODEV:	fprintf(stderr, "ENODEV"); break;
		}
		fprintf(stderr, "\n");
		fflush(stderr);
		exit(1);
	}
	else if(addr == nullptr){
		fprintf(stderr, "large_sb_alloc() mmap of size %lu returned NULL\n", size);
		fflush(stderr);
		exit(1);
	}
	return addr;
}
//todo
void BaseMeta::large_sb_retire(void* sb, size_t size){
	// assert(0&"not persistently implemented yet!");
	munmap(sb, size);
}

void* BaseMeta::malloc_from_partial(Procheap* heap){
	Descriptor* desc = nullptr;
	Anchor oldanchor,newanchor;
	uint64_t morecredits = 0;
	void* addr;

retry:
	//grab the partial desc from heap or sizeclass (exclusively)
	desc = heap_get_partial(heap);
	if(!desc){
		return nullptr;
	}
	desc->heap = heap;
	FLUSH(&desc->heap);
	oldanchor = desc->anchor.load(std::memory_order_acquire);
	TFLUSH(&desc->anchor);
	do{
		//reserve blocks
		newanchor = oldanchor;
		if(oldanchor.state == EMPTY){
			desc_retire(desc);
			goto retry;
		}
		//oldanchor state must be PARTIAL, and count must > 0
		morecredits = min(oldanchor.count - 1, MAXCREDITS);
		newanchor.count -= morecredits + 1;
		newanchor.state = (morecredits>0)?ACTIVE:FULL;
		TFLUSHFENCE;
	}while(!desc->anchor.compare_exchange_strong(oldanchor,newanchor,std::memory_order_acq_rel));
	TFLUSH(&desc->anchor);
	TFLUSHFENCE;

	oldanchor = desc->anchor.load(std::memory_order_acquire);
	TFLUSH(&desc->anchor);
	do{
		//pop reserved block
		newanchor = oldanchor;
		addr = (void*)((uint64_t)desc->sb + oldanchor.avail * desc->sz);
		newanchor.avail = *(uint64_t*)addr;
		newanchor.tag++;
		TFLUSHFENCE;
	}while(!desc->anchor.compare_exchange_strong(oldanchor,newanchor,std::memory_order_acq_rel));
	TFLUSH(&desc->anchor);
	TFLUSHFENCE;

	if(morecredits > 0){
		update_active(heap, desc, morecredits);
	}
	*((char*)addr) = (char)SMALL;
	addr += TYPE_SIZE;
	*((Descriptor**)addr) = desc;
	FLUSH(addr-TYPE_SIZE);
	FLUSH(addr);
	FLUSHFENCE;
	return ((void *)((uint64_t)addr + PTR_SIZE));

}
void* BaseMeta::malloc_from_newsb(Procheap* heap){
	Active oldactive,newactive;
	Anchor newanchor;
	void* addr = nullptr;

	Descriptor* desc = desc_alloc();
	desc->sb = small_sb_alloc();
	desc->heap = heap;
	newanchor.avail = 1;
	desc->sz = heap->sc->sz;
	desc->maxcount = heap->sc->sbsize / desc->sz;
	FLUSH(&desc->sb);
	FLUSH(&desc->heap);
	FLUSH(&desc->sz);
	FLUSH(&desc->maxcount);
	organize_blk_list(desc->sb, desc->maxcount, desc->sz);

	newactive.ptr = (uint64_t)desc>>6;
	newactive.credits = min(desc->maxcount - 1, MAXCREDITS)-1;
	newanchor.count = max(((int)desc->maxcount - 1)-((int)newactive.credits + 1), 0);
	newanchor.state = ACTIVE;
	TFLUSHFENCE;
	desc->anchor.store(newanchor,std::memory_order_release);
	TFLUSH(&desc->anchor);

	*((uint64_t*)(&oldactive)) = 0;

	TFLUSHFENCE;
	if(heap->active.compare_exchange_strong(oldactive,newactive,std::memory_order_acq_rel)){
		TFLUSH(&heap->active);
		addr = desc->sb;
		*((char*)addr) = (char)SMALL;
		addr += TYPE_SIZE;
		*((Descriptor**)addr) = desc;
		FLUSH(addr-TYPE_SIZE);
		FLUSH(addr);
		FLUSHFENCE;
		return (void*)((uint64_t)addr + PTR_SIZE);
	}
	else {
		small_sb_retire(desc->sb);
		desc_retire(desc);
		return nullptr;
	}
}
void* BaseMeta::alloc_large_block(size_t sz){
	void* addr = large_sb_alloc(sz + HEADER_SIZE, SBSIZE);
	*((char*)addr) = (char)LARGE;
	addr += TYPE_SIZE;
	*((uint64_t*)addr) = sz + HEADER_SIZE;
	FLUSH(addr-TYPE_SIZE);
	FLUSH(addr);
	FLUSHFENCE;
	return (void*)(addr + PTR_SIZE);
}