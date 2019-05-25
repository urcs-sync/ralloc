#include <sys/mman.h>

#include <string>

#include "BaseMeta.hpp"

using namespace std;
namespace pmmalloc{
	/* manager to map, remap, and unmap the heap */
	extern RegionManager* mgr;//initialized when pmmalloc constructs
	//GC
};
using namespace pmmalloc;
BaseMeta::BaseMeta(uint64_t thd_num) noexcept
: 
	avail_sb(),
	avail_desc(),
	heaps()
	// thread_num(thd_num) {
{
	/* allocate these persistent data into specific memory address */
	dirty = true;
	FLUSH(&dirty);
	/* heaps init */
	for (size_t idx = 0; idx < MAX_SZ_IDX; ++idx){
		ProcHeap& heap = heaps[idx];
		heap.partial_list.store({ nullptr });
		heap.sc_idx = idx;
		FLUSH(&heaps[idx]);
	}

	/* persistent roots init */
	for(int i=0;i<MAX_ROOTS;i++){
		roots[i]=nullptr;
		FLUSH(&roots[i]);
	}

	// warm up small sb space 
	uint64_t space_num = new_space(1);
	DBG_PRINT("allocate sb space %d\n", space_num);
	void* ptr = spaces[1][space_num].sec_start;
	organize_sb_list((void*)ptr, SB_SPACE_SIZE/SBSIZE, SBSIZE);

	FLUSHFENCE;
}

uint64_t BaseMeta::new_space(int i, size_t sz){
	// i=0:desc, i=1:small sb, i=2:large sb
	// sz is useful only when i == 2, i.e. when allocating a large block
	FLUSH(&space_num[i]);
	FLUSHFENCE;
	uint64_t my_space_num = space_num[i].fetch_add(1,std::memory_order_relaxed);
	if(my_space_num>=MAX_SECTION) assert(0&&"space number reaches max!");
	FLUSH(&space_num[i]);
	spaces[i][my_space_num].sec_bytes = 0;
	FLUSH(&spaces[i][my_space_num].sec_bytes);
	FLUSHFENCE;
	uint64_t space_size;
	switch(i){
		case 0:
			space_size = DESC_SPACE_SIZE;
			break;
		case 1:
			space_size = SB_SPACE_SIZE;
			break;
		case 2:
			space_size = sz;
			break;
		default:
			assert(0&&"incorrect space index!");
	}
	void* tmp_sec_start = nullptr;
	bool res = mgr->__nvm_region_allocator(&tmp_sec_start,PAGESIZE, space_size);
	if(!res) assert(0&&"region allocation fails!");
	spaces[i][my_space_num].sec_start = tmp_sec_start;
	spaces[i][my_space_num].sec_bytes = space_size;
	FLUSH(&spaces[i][my_space_num].sec_start);
	FLUSHFENCE;
	FLUSH(&spaces[i][my_space_num].sec_bytes);
	FLUSHFENCE;
	return my_space_num;
}

inline size_t BaseMeta::get_sizeclass(size_t size){
	return sizeclass.get_sizeclass(size);
}

inline SizeClassData* BaseMeta::get_sizeclass(ProcHeap* h){
	return get_sizeclass_by_idx(h->sc_idx);
}

inline SizeClassData* BaseMeta::get_sizeclass_by_idx(size_t idx) { 
	return sizeclass.get_sizeclass_by_idx(idx);
}

uint32_t BaseMeta::compute_idx(char* superblock, char* block, size_t sc_idx) {
	SizeClassData* sc = get_sizeclass_by_idx(sc_idx);
	uint32_t sc_block_size = sc->block_size;
	(void)sc_block_size; // suppress unused var warning

	assert(block >= superblock);
	assert(block < superblock + sc->sb_size);
	// optimize integer division by allowing the compiler to create 
	//  a jump table using size class index
	// compiler can then optimize integer div due to known divisor
	uint32_t diff = uint32_t(block - superblock);
	uint32_t idx = 0;
	switch (sc_idx) {
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

void BaseMeta::fill_cache(size_t sc_idx, TCacheBin* cache) {
	// at most cache will be filled with number of blocks equal to superblock
	size_t block_num = 0;
	// use a *SINGLE* partial superblock to try to fill cache
	malloc_from_partial(sc_idx, cache, block_num);
	// if we obtain no blocks from partial superblocks, create a new superblock
	if (block_num == 0)
		malloc_from_newsb(sc_idx, cache, block_num);

	SizeClassData* sc = get_sizeclass_by_idx(sc_idx);
	(void)sc;
	assert(block_num > 0);
	assert(block_num <= sc->cache_block_num);
}

void BaseMeta::flush_cache(size_t sc_idx, TCacheBin* cache) {
	ProcHeap* heap = &heaps[sc_idx];
	SizeClassData* sc = get_sizeclass_by_idx(sc_idx);
	uint32_t const sb_size = sc->sb_size;
	uint32_t const block_size = sc->block_size;
	// after CAS, desc might become empty and
	//  concurrently reused, so store maxcount
	uint32_t const maxcount = sc->get_block_num();
	(void)maxcount; // suppress unused warning

	// @todo: optimize
	// in the normal case, we should be able to return several
	//  blocks with a single CAS
	while (cache->get_block_num() > 0) {
		char* head = cache->peek_block();
		char* tail = head;
		PageInfo info = get_page_info_for_ptr(head);
		Descriptor* desc = info.get_desc();
		char* superblock = desc->superblock;

		// cache is a linked list of blocks
		// superblock free list is also a linked list of blocks
		// can optimize transfers of blocks between these 2 entities
		//  by exploiting existing structure
		uint32_t block_count = 1;
		// check if next cache blocks are in the same superblock
		// same superblock, same descriptor
		while (cache->get_block_num() > block_count) {
			char* ptr = *(pptr<char>*)tail;
			if (ptr < superblock || ptr >= superblock + sb_size)
				break; // ptr not in superblock

			// ptr in superblock, add to "list"
			++block_count;
			tail = ptr;
		}

		cache->pop_list(*(pptr<char>*)tail, block_count);

		// add list to desc, update anchor
		uint32_t idx = compute_idx(superblock, head, sc_idx);

		Anchor oldanchor = desc->anchor.load();
		Anchor newanchor;
		do {
			// update anchor.avail
			char* next = (char*)(superblock + oldanchor.avail * block_size);
			*(pptr<char>*)tail = next;

			newanchor = oldanchor;
			newanchor.avail = idx;
			// state updates
			// don't set SB_PARTIAL if state == SB_ACTIVE
			if (oldanchor.state == SB_FULL)
				newanchor.state = SB_PARTIAL;
			// this can't happen with SB_ACTIVE
			// because of reserved blocks
			assert(oldanchor.count < desc->maxcount);
			if (oldanchor.count + block_count == desc->maxcount) {
				newanchor.count = desc->maxcount - 1;
				newanchor.state = SB_EMPTY; // can free superblock
			}
			else
				newanchor.count += block_count;
		}
		while (!desc->anchor.compare_exchange_weak(oldanchor, newanchor));

		// after last CAS, can't reliably read any desc fields
		// as desc might have become empty and been concurrently reused
		assert(oldanchor.avail < maxcount || oldanchor.state == SB_FULL);
		assert(newanchor.avail < maxcount);
		assert(newanchor.count < maxcount);

		// CAS success
		if (newanchor.state == SB_EMPTY) {
			/* In this case, state in desc is set to be empty (aka to free)
			 * though this desc may still be in partial list.
			 * Others attempt to allocate from desc's sb will
			 * fail and help retire desc.
			 * 
			 * In this routine, we unregister desc and then retire sb.
			 * Because sb retire must happen after unregister desc,
			 * no one else would touch this part of pagemap until sb 
			 * is reallocate and thus it's safe.
			 */
			// 
			desc->in_use = false;
			FLUSH(desc);
			FLUSHFENCE;
			// unregister descriptor
			unregister_desc(heap, superblock);

			// free superblock
			small_sb_retire(superblock, get_sizeclass(heap)->sb_size);
		}
		else if (oldanchor.state == SB_FULL)
			heap_push_partial(desc);
	}
}

// (un)register descriptor pages with pagemap
// all pages used by the descriptor will point to desc in
//  the pagemap
// for (unaligned) large allocations, only first page points to desc
// aligned large allocations get the corresponding page pointing to desc
void BaseMeta::update_pagemap(ProcHeap* heap, char* ptr, Descriptor* desc, size_t sc_idx) {
	assert(ptr);

	PageInfo info;
	info.set(desc, sc_idx);

	// large allocation, don't need to (un)register every page
	// just first
	if (!heap) {
		pagemap.set_page_info(ptr, info);
		return;
	}

	// only need to worry about alignment for large allocations
	// assert(ptr == superblock);

	// small allocation, (un)register every page
	// could *technically* optimize if block_size >>> page, 
	//  but let's not worry about that
	size_t sb_size = get_sizeclass(heap)->sb_size;
	// sb_size is a multiple of page
	assert((sb_size & PAGE_MASK) == 0);
	for (size_t idx = 0; idx < sb_size; idx += PAGESIZE)
		pagemap.set_page_info(ptr + idx, info); 
}

// every time you alloc a new desc, you ought to register it
// and register_desc will flush desc and issue a fence for you
void BaseMeta::register_desc(Descriptor* desc) {
	ProcHeap* heap = desc->heap;
	char* ptr = desc->superblock;
	size_t sc_idx = 0;
	if (LIKELY(heap != nullptr))
		sc_idx = heap->sc_idx;

	update_pagemap(heap, ptr, desc, sc_idx);
	desc->in_use = true;
	FLUSH(desc);
	FLUSHFENCE;
}

// unregister descriptor before superblock deletion
// can only be done when superblock is about to be free'd to OS
inline void BaseMeta::unregister_desc(ProcHeap* heap, char* superblock) {
	update_pagemap(heap, superblock, nullptr, 0L);
}

inline PageInfo BaseMeta::get_page_info_for_ptr(void* ptr) {
	return pagemap.get_page_info((char*)ptr);
}

void BaseMeta::heap_push_partial(Descriptor* desc) {
	ProcHeap* heap = desc->heap;

	ptr_cnt<Descriptor> oldhead = heap->partial_list.load();
	ptr_cnt<Descriptor> newhead;
	do {
		newhead.set(desc, oldhead.get_counter() + 1);
		assert(oldhead.get_ptr() != newhead.get_ptr());
		newhead.get_ptr()->next_partial.store(oldhead); 
	} while (!heap->partial_list.compare_exchange_weak(oldhead, newhead));
}

Descriptor* BaseMeta::heap_pop_partial(ProcHeap* heap) {
	ptr_cnt<Descriptor> oldhead = heap->partial_list.load();
	ptr_cnt<Descriptor> newhead;
	do {
		Descriptor* olddesc = oldhead.get_ptr();
		if (!olddesc)
			return nullptr;
		newhead = olddesc->next_partial.load();
		Descriptor* desc = newhead.get_ptr();
		uint64_t counter = oldhead.get_counter();
		newhead.set(desc, counter);
	} while (!heap->partial_list.compare_exchange_weak(oldhead, newhead));
	assert(oldhead.get_ptr()->anchor.load().state!=SB_FULL);
	return oldhead.get_ptr();
}

void BaseMeta::malloc_from_partial(size_t sc_idx, TCacheBin* cache, size_t& block_num){
retry:
	ProcHeap* heap = &heaps[sc_idx];

	Descriptor* desc = heap_pop_partial(heap);
	if (!desc)
		return;

	// reserve block(s)
	Anchor oldanchor = desc->anchor.load();
	Anchor newanchor;
	uint32_t maxcount = desc->maxcount;
	uint32_t block_size = desc->block_size;
	char* superblock = desc->superblock;

	// we have "ownership" of block, but anchor can still change
	// due to free()
	do {
		if (oldanchor.state == SB_EMPTY) {
			desc_retire(desc);
			goto retry;
		}

		// oldanchor must be SB_PARTIAL
		// can't be SB_FULL because we *own* the block now
		// and it came from heap_pop_partial
		// can't be SB_EMPTY, we already checked
		// obviously can't be SB_ACTIVE
		assert(oldanchor.state == SB_PARTIAL);

		newanchor = oldanchor;
		newanchor.count = 0;
		// avail value doesn't actually matter
		newanchor.avail = maxcount;
		newanchor.state = SB_FULL;
	}
	while (!desc->anchor.compare_exchange_weak(
				oldanchor, newanchor));

	// will take as many blocks as available from superblock
	// *AND* no thread can do malloc() using this superblock, we
	//  exclusively own it
	// if CAS fails, it just means another thread added more available blocks
	//  through FlushCache, which we can then use
	uint32_t block_take = oldanchor.count;
	uint32_t avail = oldanchor.avail;

	assert(avail < maxcount);
	char* block = superblock + avail * block_size;

	// cache must be empty at this point
	// and the blocks are already organized as a list
	// so all we need do is "push" that list, a constant time op
	assert(cache->get_block_num() == 0);
	cache->push_list(block, block_take);

	block_num += block_take;
}

void BaseMeta::malloc_from_newsb(size_t sc_idx, TCacheBin* cache, size_t& block_num) {
	ProcHeap* heap = &heaps[sc_idx];
	SizeClassData* sc = get_sizeclass_by_idx(sc_idx);

	Descriptor* desc = desc_alloc();
	assert(desc);

	uint32_t const block_size = sc->block_size;
	uint32_t const maxcount = sc->get_block_num();

	desc->heap = heap;
	desc->block_size = block_size;
	desc->maxcount = maxcount;
	desc->superblock = (char*)small_sb_alloc(sc->sb_size);

	// prepare block list
	char* superblock = desc->superblock;
	for (uint32_t idx = 0; idx < maxcount - 1; ++idx) {
		pptr<char>* block = (pptr<char>*)(superblock + idx * block_size);
		char* next = superblock + (idx + 1) * block_size;
		*block = next;
	}

	// push blocks to cache
	char* block = superblock; // first block
	cache->push_list(block, maxcount);

	Anchor anchor;
	anchor.avail = maxcount;
	anchor.count = 0;
	anchor.state = SB_FULL;

	desc->anchor.store(anchor);

	assert(anchor.avail < maxcount || anchor.state == SB_FULL);
	assert(anchor.count < maxcount);

	// register new descriptor
	// must be done before setting superblock as active
	// or leaving superblock as available in a partial list
	register_desc(desc);

	// if state changes to SB_PARTIAL, desc must be added to partial list
	assert(anchor.state == SB_FULL);

	block_num += maxcount;
}

inline void BaseMeta::organize_sb_list(void* start, uint64_t count, uint64_t stride){
	// put (start)...(start+count-1) sbs to free_sb queue
	// in total it's count sbs
	uint64_t ptr = (uint64_t)start;
	for(uint64_t i = 0; i < count-1; i++){
		*(int64_t*)ptr = (int64_t)stride;
		ptr += stride;
	}
	*(int64_t*)ptr = 0;//init to empty
	ptr_cnt<char> oldhead = avail_sb.load();
	ptr_cnt<char> newhead;
	do{
		((atomic_pptr_cnt<char>*)ptr)->store(oldhead);//include counter
		newhead.set((char*)((uint64_t)start + stride), oldhead.get_counter()+1);
	}while(!avail_sb.compare_exchange_strong(oldhead,newhead));
}

void* BaseMeta::small_sb_alloc(size_t size){
	if(size != SBSIZE){
		std::cout<<"desired size: "<<size<<std::endl;
		assert(0);
	}

	char* oldptr = nullptr;

	ptr_cnt<char> oldhead = avail_sb.load();
	while(true){
		oldptr = oldhead.get_ptr();
		if(oldptr) {
			ptr_cnt<char> newhead = ((atomic_pptr_cnt<char>*)oldptr)->load();
			newhead.set(newhead.get_ptr(),oldhead.get_counter());
			if(avail_sb.compare_exchange_weak(oldhead,newhead)){
				return oldptr;
			}
		}
		else{
			uint64_t space_num = new_space(1);
			DBG_PRINT("allocate sb space %d\n", space_num);
			oldptr = spaces[1][space_num].sec_start;
			organize_sb_list((void*)((uint64_t)oldptr+SBSIZE),
				SB_SPACE_SIZE/SBSIZE - 1,
				SBSIZE);
			return oldptr;
		}
	}
}
inline void BaseMeta::small_sb_retire(void* sb, size_t size){
	assert(size == SBSIZE);

	ptr_cnt<char> oldhead = avail_sb.load();
	ptr_cnt<char> newhead;
	do{
		((atomic_pptr_cnt<char>*)sb)->store(oldhead);
		newhead.set((char*)sb, oldhead.get_counter()+1);
	} while (!avail_sb.compare_exchange_weak(oldhead,newhead));
}

/* 
 * IMPORTANT: 	Large_sb_alloc is designed for very rare 
 *				large sb (>=16K) allocations. 
 *
 *				For now it's restricted to 128 as MAX_SECTION 
 *				is 128. After 128 times, large allocation will 
 *				fail and the program will crash.
 */
void* BaseMeta::large_sb_alloc(size_t size){
	// cout<<"WARNING: Allocating a large object.\n";
	uint64_t space_num = new_space(2, size);
	DBG_PRINT("allocate large sb space %d\n", space_num);
	char* addr = spaces[2][space_num].sec_start;
	return addr;
}

void BaseMeta::large_sb_retire(void* sb, size_t size){
	// cout<<"WARNING: Deallocating a large object.\n";
	assert(size%SBSIZE == 0);//size must be a multiple of SBSIZE
	organize_sb_list(sb, size/SBSIZE, SBSIZE);
}

Descriptor* BaseMeta::desc_alloc(){
	ptr_cnt<Descriptor> oldhead = avail_desc.load();
	while(true){
		Descriptor* desc = oldhead.get_ptr();
		if (desc) {
			ptr_cnt<Descriptor> newhead = desc->next_free.load();
			newhead.set(newhead.get_ptr(), oldhead.get_counter());
			if (avail_desc.compare_exchange_weak(oldhead, newhead)) {
				assert(desc->block_size == 0);
				new (desc) Descriptor();
				return desc;
			}
		}
		else {
			// allocate several pages
			// get first descriptor, this is returned to caller
			uint64_t space_num = new_space(0);
			// cout<<"allocate desc space "<<space_num<<endl;
			// spaces[0][space_num].sec_curr.store((void*)((size_t)spaces[0][space_num].sec_start+spaces[0][space_num].sec_bytes));
			char* ptr = (char*)spaces[0][space_num].sec_start;
			Descriptor* ret = (Descriptor*)ptr;
			new (ret) Descriptor();
			// organize list with the rest of descriptors
			// and add to available descriptors
			{
				Descriptor* first = nullptr;
				Descriptor* prev = nullptr;

				char* curr_ptr = ptr + sizeof(Descriptor);
				curr_ptr = ALIGN_ADDR(curr_ptr, CACHELINE_SIZE);
				first = (Descriptor*)curr_ptr;
				while (curr_ptr + sizeof(Descriptor) <
						ptr + DESC_SPACE_SIZE) {
					Descriptor* curr = (Descriptor*)curr_ptr;
					new (curr) Descriptor();
					if (prev)
						prev->next_free.store({ curr });

					prev = curr;
					curr_ptr = curr_ptr + sizeof(Descriptor);
					curr_ptr = ALIGN_ADDR(curr_ptr, CACHELINE_SIZE);
				}

				prev->next_free.store({ nullptr });

				// add list to available descriptors
				ptr_cnt<Descriptor> oldhead = avail_desc.load();
				ptr_cnt<Descriptor> newhead;
				do {
					prev->next_free.store(oldhead);
					newhead.set(first, oldhead.get_counter() + 1);
				}
				while (!avail_desc.compare_exchange_weak(oldhead, newhead));
			}

			return ret;
		}
	}
}

inline void BaseMeta::desc_retire(Descriptor* desc){
	desc->block_size = 0;
	ptr_cnt<Descriptor> oldhead = avail_desc.load();
	ptr_cnt<Descriptor> newhead;
	do {
		desc->next_free.store(oldhead);

		newhead.set(desc, oldhead.get_counter() + 1);
	} while (!avail_desc.compare_exchange_weak(oldhead, newhead));
	desc->in_use = false;
	FLUSH(desc);
	FLUSHFENCE;
}

void* BaseMeta::alloc_large_block(size_t sz){
	void* addr = large_sb_alloc(sz);
	return addr;
}

void* BaseMeta::do_malloc(size_t size){
	if (UNLIKELY(size > MAX_SZ)) {
		// large block allocation
		size_t sbs = round_up(size, SBSIZE);//round size up to multiple of SBSIZE
		Descriptor* desc = desc_alloc();
		assert(desc);

		desc->heap = nullptr;
		desc->block_size = sbs;
		desc->maxcount = 1;
		desc->superblock = (char*)alloc_large_block(sbs);

		Anchor anchor;
		anchor.avail = 0;
		anchor.count = 0;
		anchor.state = SB_FULL;

		desc->anchor.store(anchor);

		register_desc(desc);

		char* ptr = desc->superblock;
		DBG_PRINT("large, ptr: %p", ptr);
		return (void*)ptr;
	}

	// size class calculation
	size_t sc_idx = get_sizeclass(size);

	TCacheBin* cache = &t_cache[sc_idx];
	// fill cache if needed
	if (UNLIKELY(cache->get_block_num() == 0))
		fill_cache(sc_idx, cache);

	return cache->pop_block();
}
void BaseMeta::do_free(void* ptr){
	PageInfo info = get_page_info_for_ptr(ptr);
	Descriptor* desc = info.get_desc();
	// @todo: this can happen with dynamic loading
	// need to print correct message
	assert(desc);

	size_t sc_idx = info.get_sc_idx();
 
	// DBG_PRINT("Desc %p, ptr %p", desc, ptr);

	// large allocation case
	if (UNLIKELY(!sc_idx)) {
		char* superblock = desc->superblock;

		// unregister descriptor
		unregister_desc(nullptr, superblock);
		// aligned large allocation case
		if (UNLIKELY((char*)ptr != superblock))
			unregister_desc(nullptr, (char*)ptr);

		// free superblock
		large_sb_retire(superblock, desc->block_size);

		// desc cannot be in any partial list, so it can be
		//  immediately reused
		desc_retire(desc);
		return;
	}

	TCacheBin* cache = &t_cache[sc_idx];
	SizeClassData* sc = get_sizeclass_by_idx(sc_idx);

	// flush cache if need
	if (UNLIKELY(cache->get_block_num() >= sc->cache_block_num))
		flush_cache(sc_idx, cache);

	cache->push_block((char*)ptr);
}
