#ifndef _BASE_META_HPP_
#define _BASE_META_HPP_

#include <atomic>

#include "pm_config.hpp"

#include "RegionManager.hpp"
#include "MichaelScottQueue.hpp"

struct Descriptor;

/* data structures */
struct Sizeclass{
	unsigned int sz; // block size
	unsigned int sbsize; // superblock size
	MichaelScottQueue<Descriptor*>* partial_desc_queue; // flushed only when exit
	Sizeclass(uint64_t thread_num = 1,
			unsigned int bs = 0, 
			unsigned int sbs = SBSIZE, 
			MichaelScottQueue<Descriptor*>* pdq = nullptr):
		sz(bs), 
		sbsize(sbs),
		partial_desc_queue(pdq) {
		if(partial_desc_queue == nullptr) 
			partial_desc_queue = 
			new MichaelScottQueue<Descriptor*>(thread_num);
		FLUSH(&sz);
		FLUSH(&sbsize);
		FLUSHFENCE;
	}
	void reinit_msq(uint64_t thread_num){
		if(partial_desc_queue != nullptr) {
			delete partial_desc_queue;
			partial_desc_queue = 
			new MichaelScottQueue<Descriptor*>(thread_num);
		}
	}
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
	BaseMeta(RegionManager* m, uint64_t thd_num = MAX_THREADS) : 
	mgr(m),
	free_desc(thd_num),
	thread_num(thd_num){
		FLUSH(&thread_num);
		/* allocate these persistent data into specific memory address */
		/* TODO: metadata init */

		/* persistent roots init */
		for(int i=0;i<MAX_ROOTS;i++){
			roots[i]=nullptr;
			FLUSH(&roots[i]);
		}

		/* sizeclass init */
		for(int i=0;i<MAX_SMALLSIZE/GRANULARITY;i++){
			sizeclasses[i].reinit_msq(thd_num);
			sizeclasses[i].sz = (i+1)*GRANULARITY;
			FLUSH(&sizeclasses[i]);
		}
		sizeclasses[MAX_SMALLSIZE/GRANULARITY].reinit_msq(thd_num);
		sizeclasses[MAX_SMALLSIZE/GRANULARITY].sz = 0;
		sizeclasses[MAX_SMALLSIZE/GRANULARITY].sbsize = 0;
		FLUSH(&sizeclasses[MAX_SMALLSIZE/GRANULARITY]);//the size class for large blocks

		/* processor heap init */
		for(int t=0;t<PROCHEAP_NUM;t++){
			for(int i=0;i<MAX_SMALLSIZE/GRANULARITY+1;i++){
				procheaps[t][i].sc = &sizeclasses[i];
				FLUSH(&procheaps[t][i]);
			}
		}
		FLUSHFENCE;
	}
	~BaseMeta(){
		//TODO: flush metadata back
	}
	void set_mgr(RegionManager* m){
		mgr = m;
	}
	void new_desc_space(){
		if(desc_space_num.load(std::memory_order_relaxed)>=MAX_SECTION) assert(0&&"desc space number reaches max!");
		FLUSHFENCE;
		uint64_t my_space_num = desc_space_num.fetch_add(1);
		FLUSH(&desc_space_num);
		desc_spaces[my_space_num].sec_bytes = 0;//0 if the section isn't init yet, otherwise we are sure the section is ready.
		FLUSH(&desc_spaces[my_space_num].sec_bytes);
		FLUSHFENCE;
		int res = mgr->__nvm_region_allocator(&(desc_spaces[my_space_num].sec_start),PAGESIZE, DESC_SPACE_SIZE);
		if(res != 0) assert(0&&"region allocation fails!");
		desc_spaces[my_space_num].sec_bytes = DESC_SPACE_SIZE;
		FLUSH(&desc_spaces[my_space_num].sec_start);
		FLUSH(&desc_spaces[my_space_num].sec_bytes);
		FLUSHFENCE;
	}
	void new_sb_space(){
		if(sb_space_num.load(std::memory_order_relaxed)>=MAX_SECTION) assert(0&&"sb space number reaches max!");
		FLUSHFENCE;
		uint64_t my_space_num = sb_space_num.fetch_add(1);
		FLUSH(&sb_space_num);
		sb_spaces[my_space_num].sec_bytes = 0;
		FLUSH(&sb_spaces[my_space_num].sec_bytes);
		FLUSHFENCE;
		int res = mgr->__nvm_region_allocator(&(sb_spaces[sb_space_num].sec_start),PAGESIZE, SB_SPACE_SIZE);
		if(res != 0) assert(0&&"region allocation fails!");
		sb_spaces[my_space_num].sec_bytes = SB_SPACE_SIZE;
		FLUSH(&sb_spaces[my_space_num].sec_start);
		FLUSH(&sb_spaces[my_space_num].sec_bytes);
		FLUSHFENCE;
	}
	void* set_root(void* ptr, uint64_t i){
		//this is sequential
		assert(i<MAX_ROOTS);
		void* res = nullptr;
		if(roots[i]!=nullptr) res = roots[i];
		roots[i] = ptr;
		return res;
	}
	void* get_root(uint64_t i){
		//this is sequential
		assert(i<MAX_ROOTS);
		return roots[i];
	}
};


#endif /* _BASE_META_HPP_ */