#ifndef _BASE_META_HPP_
#define _BASE_META_HPP_

#include <atomic>
#include <iostream>
#include <mutex>

#include "pm_config.hpp"
// #include "thread_util.hpp"

#include "RegionManager.hpp"
#include "SizeClass.hpp"
#include "TCache.hpp"
#include "pptr.hpp"

/********class BaseMeta********
 * This is the file where meta data structures of
 * rpmalloc are defined.
 *
 * The logic is: 
 * 	1. 	Use RegionManager to allocate a persistent 
 * 		region with size of BaseMeta
 * 	2. 	Use the pointer to the persistent region 
 * 		as BaseMeta*
 * 	3. 	Initialize BaseMeta* by calling constructor 
 * 		in place.
 *
 * Some useful functions to know:
 * 		void BaseMeta::restart():
 * 			Restart the BaseMeta after data is remapped in
 *			BaseMeta. It traces and recovers free lists.
 *		void BaseMeta::cleanup():
 *			Do cleanup before program exits.
 *			It flushes free lists and marks as clean.
 *		void set_mgr(RegionManager* m):
 *			Set mgr pointer to m.
 *			It should be called after data is remapped in
 *			BaseMeta.
 *		void* set_root(void* ptr, uint64_t i):
 *			Set persistent root i to ptr, return the old root
 *			in root i if there is, or nullptr if there isn't.
 *		void* get_root(uint64_t i):
 *			Return persistent root i, or nullptr if there isn't.
 *
 * Procedure to do a fresh start mapping to $filepath$:
 * 		BaseMeta* base_md;
 * 		RegionManager* mgr = new RegionManager(filepath);
 * 		bool res = mgr->__nvm_region_allocator((void**)&base_md,PAGESIZE,sizeof(BaseMeta));
 * 		if(!res) assert(0&&"mgr allocation fails!");
 * 		mgr->__store_heap_start(base_md);
 * 		new (base_md) BaseMeta(mgr);
 *
 * Procedure to restart from $filepath$:
 * 		BaseMeta* base_md;
 * 		RegionManager* mgr = new RegionManager(filepath);
 * 		void* hstart = mgr->__fetch_heap_start();
 * 		base_md = (BaseMeta*) hstart;
 * 		base_md->set_mgr(mgr);
 * 		base_md->restart();
 *
 * By default free_sb is mapped to $(HEAPFILE_PREFIX)_stack_rpmalloc_freesb,
 * free_desc to $(HEAPFILE_PREFIX)_queue_rpmalloc_freedesc, 
 * and partial_desc of each sizeclass with block size sz to 
 * $(HEAPFILE_PREFIX)_queue_scpartial$(sz).
 *
 * Most of functions related to malloc and free share large portion of 
 * code with the open source project https://github.com/ricleite/lrmalloc
 * Some modifications were applied for bug fixing or functionality adjustment.
 *
 * Adapted and reimplemented by:
 * 		Wentao Cai (wcai6@cs.rochester.edu)
 */

// pointers to the block in region idx.
// relative address stored in CrossPtr is the offset 
// from the start of that region to the block.
class BaseMeta;
namespace rpmalloc{
	/* manager to map, remap, and unmap the heap */
	extern Regions* _rgs;//initialized when rpmalloc constructs
	extern bool initialized;
	extern BaseMeta* base_md;
	extern void public_flush_cache();
	//GC
};
template<class T, RegionIndex idx>
class CrossPtr {
public:
	char* off;
	CrossPtr(T* real_ptr = nullptr) noexcept;
	inline operator T*() const{// cast to absolute pointer
		if(UNLIKELY(is_null())){
			return nullptr;
		} else{
			return reinterpret_cast<T*>(rpmalloc::_rgs->translate(idx, off));
		}
	} 
	inline operator void*() const{
		return reinterpret_cast<void*>(static_cast<T*>(*this));
	}
	T& operator* ();
	T* operator-> ();
	inline CrossPtr& operator= (const CrossPtr<T,idx> &p){
		off = p.off;
		return *this;
	}
	inline CrossPtr& operator= (const T* p){
		uint64_t tmp = reinterpret_cast<uint64_t>(p);//get rid of const
		off = rpmalloc::_rgs->untranslate(idx, reinterpret_cast<char*>(tmp));
		return *this;
	}
	inline CrossPtr& operator= (const void* p){
		uint64_t tmp = reinterpret_cast<uint64_t>(p);//get rid of const
		off = rpmalloc::_rgs->untranslate(idx, reinterpret_cast<char*>(tmp));
		return *this;
	}
	inline CrossPtr& operator= (const std::nullptr_t& p){
		off = nullptr; return *this;
	}
	inline bool is_null() const{
		return off == nullptr;
	}
};

template<class T, RegionIndex idx>
inline bool operator==(const CrossPtr<T,idx>& lhs, const std::nullptr_t& rhs){
	return lhs.is_null();
}

template<class T, RegionIndex idx>
inline bool operator==(const CrossPtr<T,idx>& lhs, const CrossPtr<T,idx>& rhs){
	return static_cast<T*>(lhs) == static_cast<T*>(rhs);
}

template <class T, RegionIndex idx>
inline bool operator!=(const CrossPtr<T,idx>& lhs, const std::nullptr_t& rhs){
	return !lhs.is_null();
}


template<class T, RegionIndex idx>
class AtomicCrossPtr {
public:
	std::atomic<char*> off;
	AtomicCrossPtr(T* real_ptr = nullptr) noexcept;
	T* load(std::memory_order order = std::memory_order_seq_cst) const noexcept;
	void store(T* desired, 
		std::memory_order order = std::memory_order_seq_cst ) noexcept;
	bool compare_exchange_weak(T*& expected, T* desired,
		std::memory_order order = std::memory_order_seq_cst ) noexcept;
	bool compare_exchange_strong(T*& expected, T* desired,
		std::memory_order order = std::memory_order_seq_cst ) noexcept;
};

template<class T, RegionIndex idx>
class AtomicCrossPtrCnt {
public:
	std::atomic<char*> off;
	AtomicCrossPtrCnt(T* real_ptr = nullptr, uint64_t counter = 0) noexcept;
	ptr_cnt<T> load(std::memory_order order = std::memory_order_seq_cst) const noexcept;
	void store(ptr_cnt<T> desired, 
		std::memory_order order = std::memory_order_seq_cst ) noexcept;
	bool compare_exchange_weak(ptr_cnt<T>& expected, ptr_cnt<T> desired,
		std::memory_order order = std::memory_order_seq_cst ) noexcept;
	bool compare_exchange_strong(ptr_cnt<T>& expected, ptr_cnt<T> desired,
		std::memory_order order = std::memory_order_seq_cst ) noexcept;
};

// superblock states
// used in Anchor::state
enum SuperblockState {
	// invalid state
	SB_ERROR	= 0,
	// all blocks allocated or reserved
	SB_FULL		= 1,
	// has unreserved available blocks
	SB_PARTIAL	= 2,
	// all blocks are free
	SB_EMPTY	= 3,
};

struct Anchor;
struct DescriptorNode;
struct Descriptor;
struct ProcHeap;

/* data structures */
struct Anchor{
	uint64_t avail:31,count:31, state:2;
	Anchor(uint64_t a = 0) noexcept {(*(uint64_t*)this) = a;}
	Anchor(unsigned a, unsigned c, unsigned s) noexcept :
		avail(a),count(c),state(s){};
};
static_assert(sizeof(Anchor) == sizeof(uint64_t), "Invalid anchor size");

/* Superblock descriptor
 * needs to be cache-line aligned
 * descriptors are allocated and *never* freed
 */
struct Descriptor {
	// free superblocks are linked by their descriptors
	RP_TRANSIENT atomic_pptr<Descriptor> next_free;
	// used in partial descriptor list
	RP_TRANSIENT atomic_pptr<Descriptor> next_partial;
	// anchor; is reconstructed during recovery
	RP_TRANSIENT std::atomic<Anchor> anchor;

	RP_PERSIST CrossPtr<char, SB_IDX> superblock;
	RP_PERSIST CrossPtr<ProcHeap, META_IDX> heap;
	RP_PERSIST uint32_t block_size; // block size acquired from sc
	RP_PERSIST uint32_t maxcount; // block number acquired from sc
	Descriptor() noexcept :
		next_free(),
		next_partial(),
		anchor()，
		superblock(),
		heap(),
		block_size(),
		maxcount(){};
}__attribute__((aligned(CACHELINE_SIZE)));
static_assert(sizeof(Descriptor) == CACHELINE_SIZE, "Invalid Descriptor size");

// at least one ProcHeap instance exists for each sizeclass
struct ProcHeap {
public:
	// ptr to descriptor, head of partial descriptor list
	RP_TRANSIENT AtomicCrossPtrCnt<Descriptor, DESC_IDX> partial_list;
	/* size class index; never change after init
	 * though it's tagged RP_PERSIST, in 1/sc scheme,
	 * we don't have to flush it at all; it's fixed.
	 */
	RP_PERSIST size_t sc_idx;
	// std::mutex lk;
	ProcHeap() noexcept :
		partial_list(){};
}__attribute__((aligned(CACHELINE_SIZE)));

class BaseMeta {
	// unused small sb
	RP_TRANSIENT AtomicCrossPtrCnt<Descriptor, DESC_IDX> avail_sb;
	RP_PERSIST bool dirty;

	// so far we don't need thread_num at all
	// RP_PERSIST uint64_t thread_num;
	/* 1 heap per sc, and don't have to be persistent in this scheme
	 * todo: alloc a transient region for it in order to share among APPs
	 */
	RP_PERSIST ProcHeap heaps[MAX_SZ_IDX];
	//persistent root
	RP_PERSIST CrossPtr<char, SB_IDX> roots[MAX_ROOTS] = {nullptr};//gc_ptr_base*
public:
	BaseMeta() noexcept;
	~BaseMeta(){
		/* usually BaseMeta shouldn't be destructed, 
		 * and will be reused in the next time
		 */
		std::cout<<"Warning: BaseMeta is being destructed!\n";
		cleanup();
	}
	void* do_malloc(size_t size);
	void do_free(void* ptr);
	inline uint64_t min(uint64_t a, uint64_t b){return a>b?b:a;}
	inline uint64_t max(uint64_t a, uint64_t b){return a>b?a:b;}
	inline uint64_t round_up(uint64_t numToRound, uint64_t multiple) {
		//only works for some multiple that is a power of 2
		assert(multiple && ((multiple & (multiple - 1)) == 0));
		return (numToRound + multiple - 1) & ~(multiple - 1);
	}
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
	void restart(){
		// free_desc = new ArrayQueue<Descriptor*>("rpmalloc_freedesc");
		// free_sb = new ArrayStack<void*>("rpmalloc_freesb");
		// todo: GC
		assert(!dirty && "Heap is dirty and GC isn't implemented!");
		dirty = true;
	}
	void cleanup(){
		// give back tcached blocks
		rpmalloc::public_flush_cache();
		char* addr = reinterpret_cast<char*>(this);
		// flush values in BaseMeta, including avail_sb and partial lists
		for(size_t i = 0; i < sizeof(BaseMeta); i += CACHELINE_SIZE) {
			addr += CACHELINE_SIZE;
			FLUSH(addr);
		}
		FLUSHFENCE;
		dirty = false;
		FLUSH(&dirty);
		FLUSHFENCE;
	}

private:
	/*
	 * i=0:desc, i=1:small sb, i=2:large sb. 
	 * return index of allocated space
	 *
	 * sz is useful only when i == 2, 
	 * i.e. when allocating a large block
	 */
	void* expand_sb(size_t sz);
	void expand_small_sb();
	void* expand_get_small_sb();
	void* expand_get_large_sb(size_t sz);

	// func on size class
	size_t get_sizeclass(size_t size);
	SizeClassData* get_sizeclass(ProcHeap* h);
	SizeClassData* get_sizeclass_by_idx(size_t idx);
	// compute block index in superblock by addr to sb, block, and sc index
	uint32_t compute_idx(char* superblock, char* block, size_t sc_idx);

	// func on cache
	void fill_cache(size_t sc_idx, TCacheBin* cache);
public://we need to call this function to flush TLS cache during exit
	void flush_cache(size_t sc_idx, TCacheBin* cache);
private:
	// func on page map
	// find desc of the block
	Descriptor* desc_lookup(char* ptr);
	inline Descriptor* desc_lookup(void* ptr){return desc_lookup(reinterpret_cast<char*>(ptr));}
	char* sb_lookup(Descriptor* desc);

	// helper func
	void heap_push_partial(Descriptor* desc);
	Descriptor* heap_pop_partial(ProcHeap* heap);
	// fill cache from a partially used sb in heap[sc_idx]
	void malloc_from_partial(size_t sc_idx, TCacheBin* cache, size_t& block_num);
	// fill cache by allocating a new sb in heap[sc_idx]
	void malloc_from_newsb(size_t sc_idx, TCacheBin* cache, size_t& block_num);
	// alloc function to call for large block
	void* alloc_large_block(size_t sz);

	// add all newly allocated sbs to free_sb
	void organize_sb_list(void* start, uint64_t count);
	// get one free sb or allocate a new space for sbs
	void* small_sb_alloc(size_t size);
	// free the superblock sb points to
	void small_sb_retire(void* sb, size_t size);

	// allocate a large sb
	void* large_sb_alloc(size_t size);
	// retire a large sb
	void large_sb_retire(void* sb, size_t size);

	// get unused desc from avail_desc or allocate a new space for desc
	Descriptor* desc_alloc();
	// put desc to avail_desc and flush it as unused
	void desc_retire(Descriptor* desc);
}__attribute__((aligned(CACHELINE_SIZE)));


#endif /* _BASE_META_HPP_ */