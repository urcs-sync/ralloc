/*

Copyright 2017 University of Rochester

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. 

*/


#ifndef BIPTR_HPP
#define BIPTR_HPP

template<class T> struct fat_pointer_rec{
	// T* ptr;
	// uint64_t tag;
	std::atomic<T*> ptr;
	std::atomic<uint64_t> tag;
	fat_pointer_rec(T* p, uint64_t t){
		ptr.store(p, std::memory_order_release);
		tag.store(t, std::memory_order_release);
	}
	fat_pointer_rec(){
		ptr.store(nullptr, std::memory_order_release);
		tag.store(0, std::memory_order_release);
	}
};

template<class T> class FatPtr{
public:
	fat_pointer_rec<T> fat_p;

// public:
	FatPtr(T* p, uint64_t t): fat_p(p, t){}
	FatPtr(){}
	//inline bool WideCAS(fat_pointer_rec<T> *addr, fat_pointer_rec<T> &old_value, fat_pointer_rec<T> &new_value) {

#if (__x86_64__ || __ppc64__)
	inline bool WideCAS(fat_pointer_rec<T> &old_value, 
		fat_pointer_rec<T> &new_value, std::memory_order morder) {
		bool ret;
		__asm__ __volatile__(
		"lock cmpxchg16b %1;\n"
		"sete %0;\n"
		:"=m"(ret),"+m" (*(volatile fat_pointer_rec<T> *) (&fat_p))
		:"a" (old_value.ptr), "d" (old_value.tag), "b" (new_value.ptr), "c" (new_value.tag));
		std::atomic_thread_fence(morder);
		return ret;
	}
#else
	inline bool WideCAS(fat_pointer_rec<T> &old_value, 
		fat_pointer_rec<T> &new_value, std::memory_order morder) {
		errexit("WCAS not supported with -m32.");
	}
#endif

	inline bool WideCAS(fat_pointer_rec<T> &old_value, fat_pointer_rec<T> &new_value){
		return WideCAS(old_value, new_value, std::memory_order_release);
	}
	inline T* load_ptr(std::memory_order morder){
		// std::atomic_thread_fence(morder);
		// return fat_p.ptr;
		return fat_p.ptr.load(morder);
	}
	inline T* load_ptr(){
		return load_ptr(std::memory_order_acquire);
	}
	inline void store_ptr(T* &p, std::memory_order morder){
		// fat_p.ptr = p;
		// std::atomic_thread_fence(morder);
		fat_p.ptr.store(p, morder);
	}
	inline void store_ptr(T* &p){
		store_ptr(p, std::memory_order_release);
	}
	inline uint64_t load_tag(std::memory_order morder){
		return fat_p.tag.load(morder);
	}
	inline uint64_t load_tag(){
		return load_tag(std::memory_order_acquire);
	}
	inline void store_tag(uint64_t t, std::memory_order morder){
		fat_p.tag.store(t, morder);
	}	
	inline void store_tag(uint64_t t){
		store_tag(t, std::memory_order_release);
	}
};

template<class T> class biptr{
private:
	std::atomic<uint64_t>* birth_before;
	std::atomic<T*>* p;
	//FatPtr typed 128 bit record
	FatPtr<T> fat_ptr;

public:
	static RangeTracker<T>* range_tracker;

	biptr<T>(){
		// p->store(NULL, std::memory_order_release);
		// birth_before->store(get_birth_epoch(NULL), std::memory_order_release);
		birth_before = &(fat_ptr.fat_p.tag);
		p = &(fat_ptr.fat_p.ptr);
	}
	biptr<T>(T* obj): fat_ptr(obj, get_birth_epoch(obj)){
		// p->store(obj, std::memory_order_release);
		// birth_before->store(get_birth_epoch(obj), std::memory_order_release);
		birth_before = &(fat_ptr.fat_p.tag);
		p = &(fat_ptr.fat_p.ptr);

	}
	biptr<T>(biptr<T> &other): fat_ptr(other.ptr(), other.birth()){
		// if (range_tracker->type != WCAS){
		// 	p->store(other.ptr(), std::memory_order_release);
		// 	birth_before->store(other.birth(), std::memory_order_release);
		// }
		birth_before = &(fat_ptr.fat_p.tag);
		p = &(fat_ptr.fat_p.ptr);
	}

	static void set_tracker(RangeTracker<T>* tracker){
		range_tracker = tracker;
	}

	inline uint64_t get_epoch(){
		return range_tracker->get_epoch();
	}

	inline uint64_t birth(){
		// return range_tracker->type == WCAS? 
		// 	fat_ptr.load_tag() : birth_before->load(std::memory_order_acquire);
		return fat_ptr.load_tag();
	}
	inline T* ptr() {
		// return range_tracker->type == WCAS?
		// 	fat_ptr.load_ptr() : p->load(std::memory_order_acquire);
		return fat_ptr.load_ptr();
	}
	inline T* load(){
		return ptr();
	}
	inline uint64_t get_birth_epoch(T* obj){
		T* ptr = (T*) ((size_t)obj & 0xfffffffffffffffc); //in case the last bit is toggled.
		return (ptr)? RangeTracker<T>::read_birth(ptr) : 0;
	}

	inline bool CAS(T* &ori, T* obj, std::memory_order morder){
		if (range_tracker->type != WCAS){
			uint64_t e_ori = birth_before->load(std::memory_order_acquire);
			uint64_t birth_epoch = get_birth_epoch(obj);
			switch (range_tracker->type){
				case LF:
					while(true){
						if (e_ori < birth_epoch){
							if (birth_before->compare_exchange_weak(e_ori, 
								birth_epoch, morder, std::memory_order_relaxed)){
								break;
							}
						} else {
							break;
						}
					}
					break;
				case FAA:
					if (e_ori < birth_epoch){
						birth_before->fetch_add(birth_epoch - e_ori, std::memory_order_acq_rel);
					}
					break;
				default:
					break;
			}
			return p->compare_exchange_strong(ori, obj, morder,
				std::memory_order_relaxed);
		} else { // WCAS
			fat_pointer_rec<T> old_value(ori, fat_ptr.load_tag());
			fat_pointer_rec<T> new_value(obj, get_birth_epoch(obj));
			return fat_ptr.WideCAS(old_value, new_value);
		}
	}

	inline bool CAS(T* &ori, T* obj){
		return CAS(ori, obj, std::memory_order_release);
	}

	inline T* protect_and_fetch_ptr(){
		while(true){
			range_tracker->update_reserve(birth());
			T* ret = this->ptr();
			//if (range_tracker->validate(get_birth_epoch(ret))){
			if (range_tracker->validate(birth())){
				return (ret);
			}
		}
	}

	inline biptr<T>& store(T* obj){
		if (range_tracker->type != WCAS){
			this->birth_before->store(get_birth_epoch(obj), std::memory_order_relaxed);
			this->p->store(obj, std::memory_order_relaxed);
		} else {
			fat_pointer_rec<T> old_value(fat_ptr.load_ptr(), fat_ptr.load_tag());
			fat_pointer_rec<T> new_value(obj, get_birth_epoch(obj));
			while(!this->fat_ptr.WideCAS(old_value, new_value));
		}
		return *this;
	}

	//Please note that operator = is not thread safe due to separate update of p and birth_before
	//The WCAS version is thread safe.
	inline biptr<T>& operator = (const biptr<T> &other){
		if (this != &other){
			//while(!this->CAS(other.ptr()));
			if (range_tracker->type != WCAS){
				this->birth_before->store(other.birth(), std::memory_order_relaxed);
				this->p->store(other.ptr(), std::memory_order_relaxed);
			} else {
				fat_pointer_rec<T> old_value(fat_ptr.load_ptr(), fat_ptr.load_tag());
				fat_pointer_rec<T> new_value(other.ptr(), other.birth());
				while(!this->fat_ptr.WideCAS(old_value, new_value));
			}
		}
		return *this;
	}
	inline biptr<T>& operator = (T* obj){
		return store(obj);
	}
	inline bool operator == (void* obj){
		return (this->ptr() == obj);
	}
	inline bool operator != (void* obj){
		return (this->ptr() != obj);
	}
	inline T* operator -> (){
		return protect_and_fetch_ptr();
	}
	inline T& operator * (){
		return *protect_and_fetch_ptr();
	}
	inline explicit operator bool(){
		return (this->ptr()!=NULL);
	}

};

#endif