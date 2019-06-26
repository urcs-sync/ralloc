#ifndef _GC_HPP_
#define _GC_HPP_

#include <unordered_set>
#include "pptr.hpp"

#include "pm_config.hpp"
#include "RegionManager.hpp"
#include "BaseMeta.hpp"

namespace rpmalloc{
	BaseMeta* base_md;
	Regions* _rgs;
}
class GarbageCollection{
public:
	GarbageCollection(){};
	void operator() () {
		DBG_PRINT("Start garbage collection...\n");

		/* todo
		// Step 1: mark all accessible blocks from roots
		// Step 2: set desc accordingly
		// Step 3: reconstruct partial list
		// Step 4: reconstruct free list
		todo */

		DBG_PRINT("Garbage collection Completed!\n");
	}
	struct gc_ptr_base{
		void* ptr;
		size_t sz;
		gc_ptr_base(void* p, size_t s):ptr(p), sz(s){};
		virtual void filter_func() {
			char* curr = reinterpret_cast<char*>(ptr);
			for(int i=0;i<sz;i++){
				mark_func(*reinterpret_cast<pptr<void>*>(curr));
				curr++;
			}
		}
	};

	// persistent roots are gc_ptr with cross to be true
	template<class T>
	struct gc_ptr : public gc_ptr_base{
		gc_ptr(T* v){
			ptr = (void*)v;
			Descriptor* desc = rpmalloc::base_md->desc_lookup((char*)v);
			sz = desc->block_size();
		};
		gc_ptr(char* v){
			ptr = (void*)v;
			Descriptor* desc = rpmalloc::base_md->desc_lookup(v);
			sz = desc->block_size();
		};
		operator T*(){return (T*)ptr;};//cast to transient pointer
		T& operator *(){return *(T*)ptr;}//dereference
		T* operator ->(){return (T*)ptr;}//arrow
		virtual void filter_func(){
			return gc_ptr_base::filter_func();
		}
	};

	template<class T>
	void mark_func(const pptr<T>& ptr){
		void* addr = static_cast<void*>(ptr);
		// Step 1: check if it's a valid pptr
		if(UNLIKELY(!rpmalloc::_rgs->in_range(SB_IDX, addr))) 
			return; // return if not in range
		// Step 2: mark potential pptr
		marked_blk.insert(addr);
		// Step 3: construct gc_ptr<T> from ptr and call its filter_func
		gc_ptr<T> gc_p(addr);
		gc_p->filter_func();
	};

	std::unordered_set<void*> marked_blk;
};

#endif