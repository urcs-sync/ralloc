#ifndef _BASE_META_HPP_
#define _BASE_META_HPP_

#include <atomic>
#include <iostream>

#include "pm_config.hpp"

#include "RegionManager.hpp"
#include "MichaelScottQueue.hpp"

struct Descriptor;

/* data structures */
struct Sizeclass{
	unsigned int sz; // block size
	unsigned int sbsize; // superblock size
	MichaelScottQueue<Descriptor*>* partial_desc; // flushed only when exit
	Sizeclass(uint64_t thread_num = 1,
			unsigned int bs = 0, 
			unsigned int sbs = SBSIZE, 
			MichaelScottQueue<Descriptor*>* pdq = nullptr);
	void reinit_msq(uint64_t thread_num);
}__attribute__((aligned(CACHE_LINE_SIZE)));

struct Active{
	uint64_t ptr:58, credits:6;
	Active(){ptr=0;credits=0;}
	Active(uint64_t in){ptr=in>>6;credits=in&0x3f;}
};

struct Procheap {
	Sizeclass* sc;					// pointer to parent sizeclass
	atomic<Active> active;			// initially NULL; flushed only when exit
	atomic<Descriptor*> partial;	// initially NULL, pointer to the partially used sb's desc; flushed only when exit
	Procheap(Sizeclass* s = nullptr,
			uint64_t a = 0,
			Descriptor* p = nullptr):
		sc(s),
		active(a),
		partial(p) {};
}__attribute__((aligned(CACHE_LINE_SIZE)));

struct Anchor{
	uint64_t avail:24,count:24, state:2, tag:14;
	Anchor(uint64_t a = 0){*this = a;}
	Anchor(unsigned a, unsigned c, unsigned s, unsigned t):
		avail(a),count(c),state(s),tag(t){};
};

struct Descriptor{
	atomic<Anchor> anchor;
	void* sb;				// pointer to superblock
	Procheap* heap;			// pointer to owner procheap
	unsigned int sz;		// block size
	unsigned int maxcount;	// superblock size / sz
}__attribute__ ((aligned (64))); //align to 64 so that last 6 of active can use for credits

struct Section {
	void* sec_start;
	size_t sec_bytes;
};

class BaseMeta{
	/* transient metadata and tools */
	RegionManager* mgr;//assigned when BaseMeta constructs
	MichaelScottQueue<Descriptor*> free_desc;

	/* persistent metadata defined here */
	//base metadata
	uint64_t thread_num;
	//TODO: other metadata

	void* roots[MAX_ROOTS];//persistent root
	Sizeclass sizeclasses[MAX_SMALLSIZE/GRANULARITY+1];
	Procheap procheaps[PROCHEAP_NUM][MAX_SMALLSIZE/GRANULARITY+1];

	std::atomic<uint64_t> desc_space_num = 0;
	Section desc_spaces[MAX_SECTION];
	std::atomic<uint64_t> sb_space_num = 0;
	Section sb_spaces[MAX_SECTION];
	/* persistent metadata ends here */
public:
	BaseMeta(RegionManager* m, uint64_t thd_num = MAX_THREADS);
	~BaseMeta(){
		//usually BaseMeta shouldn't be destructed, and will be reused in the next time
		std::cout<<"Warning: BaseMeta is being destructed!\n";
	}
	inline void set_mgr(RegionManager* m){mgr = m;}
	void new_desc_space();
	void new_sb_space();
	inline void* set_root(void* ptr, uint64_t i){
		//this is sequential
		assert(i<MAX_ROOTS);
		void* res = nullptr;
		if(roots[i]!=nullptr) res = roots[i];
		roots[i] = ptr;
		FLUSH(&roots[i]);
		FLUSHFENCE;
		return res;
	}
	inline void* get_root(uint64_t i){
		//this is sequential
		assert(i<MAX_ROOTS);
		return roots[i];
	}
	bool flush(){
		//flush everything before exit
		//currently we just assume everything will be automatically flushed back when exit.
	}

	//TODO below
	void* alloc_sb(size_t size, uint64_t alignement);
	void organize_desc_list(Descriptor* start, uint64_t count, uint64_t stride);// put new descs to free_desc queue
	void organize_sb_list(void* start, uint64_t count, uint64_t stride);//create linked freelist of the sb
	
	Descriptor* desc_alloc();
	void desc_retire(Descriptor* desc);
	Descriptor* list_get_partial(Sizeclass* sc);//get a partial desc
	void list_put_partial(Descriptor* desc);//put a partial desc to partial_desc
	Descriptor* heap_get_partial(Procheap* heap);
	void heap_put_partial(Descriptor* desc);
	void list_remove_empty_desc(Sizeclass* sc);//try to retire an empty desc from sc->partial_desc
	void remove_empty_desc(Procheap* heap, Descriptor* desc);//remove the empty desc from heap->partial, or run list_remove_empty_desc on heap->sc
	void update_active(Procheap* heap, Descriptor* desc, uint64_t morecredits);
	Descriptor* mask_credits(Active oldactive);

	Procheap* find_heap(size_t sz);
	void* malloc_from_active(Procheap* heap);
	void* malloc_from_partial(Procheap* heap);
	void* malloc_from_newsb(Procheap* heap);
	void* alloc_large_block(size_t sz);
};


#endif /* _BASE_META_HPP_ */