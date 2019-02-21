#include "BaseMeta.hpp"

Sizeclass::Sizeclass(uint64_t thread_num = 1,
		unsigned int bs = 0, 
		unsigned int sbs = SBSIZE, 
		MichaelScottQueue<Descriptor*>* pdq = nullptr):
	sz(bs), 
	sbsize(sbs),
	partial_desc(pdq) {
	if(partial_desc == nullptr) 
		partial_desc = 
		new MichaelScottQueue<Descriptor*>(thread_num);
	FLUSH(&sz);
	FLUSH(&sbsize);
	FLUSHFENCE;
}

void Sizeclass::reinit_msq(uint64_t thread_num){
	if(partial_desc != nullptr) {
		delete partial_desc;
		partial_desc = 
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


void* BaseMeta::alloc_sb(size_t size, uint64_t alignement);
void BaseMeta::organize_desc_list(Descriptor* start, uint64_t count, uint64_t stride){
	// put new descs to free_desc queue
	uint64_t ptr = (uint64_t)start;
	int tid = get_thread_id();
	for(uint64_t i = 1; i < count; i++){
		ptr += stride;
		free_desc.enqueue((Descriptor*)ptr, tid);
	}

}
void BaseMeta::organize_sb_list(void* start, uint64_t count, uint64_t stride){
//create linked freelist of the sb
	uint64_t ptr = (uint64_t)start; 
	for (uint64_t i = 0; i < count - 1; i++) {
		*((uint64_t*)ptr) = ptr + stride;
		ptr += stride;
	}
	*((uint64_t*)ptr) = 0;
}


Descriptor* BaseMeta::desc_alloc(){
	Descriptor* desc = nullptr;
	int tid = get_thread_id();
	if(auto tmp = free_desc.dequeue(tid)){
		desc = tmp.value();
	}
	else {
		desc = alloc_sb(DESCSBSIZE, sizeof(Descriptor));
		organize_desc_list(desc, DESCSBSIZE/sizeof(Descriptor), sizeof(Descriptor));
	}
	return desc;
}
inline void BaseMeta::desc_retire(Descriptor* desc){
	free_desc.enqueue(desc, tid);
}
Descriptor* BaseMeta::list_get_partial(Sizeclass* sc){
	int tid = get_thread_id();//todo
	auto res = sc->partial_desc.dequeue(tid);
	if(res) return res.value();
	else return nullptr;
}
inline void BaseMeta::list_put_partial(Descriptor* desc){
	int tid = get_thread_id();//todo
	desc->heap->sc->partial_desc.enqueue(desc,tid);
}
Descriptor* BaseMeta::heap_get_partial(Procheap* heap){
	Descriptor* desc = heap->partial.load();
	do{
		if(desc == nullptr){
			return list_get_partial(heap->sc);
		}
	}while(!heap->partial.compare_exchange_weak(desc,nullptr));
	return desc;
}
void BaseMeta::heap_put_partial(Descriptor* desc){
	Descriptor* prev = desc->heap->partial.load();
	while(!desc->heap->partial.compare_exchange_weak(prev,desc));
	if(prev){
		list_put_partial(prev);
	}
}
void BaseMeta::list_remove_empty_desc(Sizeclass* sc){
	//try to retire empty descs from sc->partial_desc until reaches nonempty
	Descriptor* desc;
	int num_non_empty = 0;
	int tid = get_thread_id();
	while(auto tmp = sc->partial_desc.dequeue(tid)){
		desc = tmp.value();
		if(desc->sb == nullptr){
			desc_retire(desc);
		}
		else {
			sc->partial_desc.enqueue(desc,tid);
			if(++num_non_empty >= 2) break;
		}
	}
}
void BaseMeta::remove_empty_desc(Procheap* heap, Descriptor* desc){
	//remove the empty desc from heap->partial, or run list_remove_empty_desc on heap->sc
	if(heap->partial.compare_exchange_strong(desc,nullptr)) {
		desc_retire(desc);
	}
	else {
		list_remove_empty_desc(heap->sc);
	}
}
void BaseMeta::update_active(Procheap* heap, Descriptor* desc, uint64_t morecredits){
	Active oldactive, newactive;
	Anchor oldanchor, newanchor;
	
	*((uint64_t*)&oldactive) = 0;
	newactive.ptr = (uint64_t)desc>>6;
	newactive.credits = morecredits - 1;
	if(heap->active.compare_exchange_strong(oldactive,newactive)){
		return;
	}
	oldanchor = desc->anchor.load();
	do{
		newanchor = oldanchor;
		newanchor.count += morecredits;
		newancho.state = PARTIAL;
	}while(!desc->anchor.compare_exchange_weak(oldanchor,newanchor));
	heap_put_partial(desc);
}
inline Descriptor* BaseMeta::mask_credits(Active oldactive){
	uint64_t ret = oldactive.ptr;
	return (Descriptor*)(ret<<6);
}


Procheap* BaseMeta::find_heap(size_t sz){
	// We need to fit both the object and the descriptor in a single block
	sz += HEADER_SIZE;
	if (sz > 2048) {
		return nullptr;
	}
	int tid = get_thread_id();
	return &procheaps[tid][sz / GRANULARITY];
}
void* BaseMeta::malloc_from_active(Procheap* heap);
void* BaseMeta::malloc_from_partial(Procheap* heap);
void* BaseMeta::malloc_from_newsb(Procheap* heap);
void* BaseMeta::alloc_large_block(size_t sz);



