#include "BaseMeta.hpp"

Sizeclass::Sizeclass(uint64_t thread_num = 1,
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

void Sizeclass::reinit_msq(uint64_t thread_num){
	if(partial_desc_queue != nullptr) {
		delete partial_desc_queue;
		partial_desc_queue = 
		new MichaelScottQueue<Descriptor*>(thread_num);
	}
}

BaseMeta::BaseMeta(RegionManager* m, uint64_t thd_num = MAX_THREADS) : 
	mgr(m),
	free_desc(thd_num),
	thread_num(thd_num) {
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

void BaseMeta::new_desc_space(){
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

void BaseMeta::new_sb_space(){
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
