#ifndef _PPTR_HPP_
#define _PPTR_HPP_

#include <vector>
#include <iostream>
#include <cstddef>
#include <atomic>
#include "gc.hpp"
using namespace std;

class ptr_base{
protected:
	int64_t off;
public:
	// void* val;
	ptr_base(int64_t v=0)noexcept{
		off = v;
	}
	
	// template<class T>
	// operator T*(){return static_cast<T*>(val);}//cast to transient pointer
};

template<class T>
class pptr : public ptr_base{
public:
	pptr(T* v=nullptr)noexcept: //default constructor
		ptr_base(v==nullptr ? 0 : ((int64_t)v) - ((int64_t)this)) {};
	pptr(const pptr<T> &p)noexcept: //copy constructor
		ptr_base(p.is_null() ? 0 : ((int64_t)(p.off + (int64_t)&p)) - ((int64_t)this)) {};
	inline operator T*() const{ //cast to transient pointer
		return off==0 ? nullptr : (T*)(off + ((int64_t)this));
	}
	inline operator void*() const{ //cast to transient pointer
		return off==0 ? nullptr : (void*)(off + ((int64_t)this));
	}
	inline T& operator * () { //dereference
		return *(T*)(off + ((int64_t)this));
	}
	inline T* operator -> (){ //arrow
		return (T*)(off + ((int64_t)this));
	}
	inline pptr& operator = (const pptr &p){ //assignment
		off = ((int64_t)(p.off + (int64_t)&p)) - ((int64_t)this);
		return *this;
	}
	inline pptr& operator = (const T* p){ //assignment
		off = ((int64_t)p) - ((int64_t)this);
		return *this;
	}
	inline pptr& operator = (const void* p){ //assignment
		off = ((int64_t)p) - ((int64_t)this);
		return *this;
	}
	inline pptr& operator = (const std::nullptr_t& p){ //assignment
		off = 0;
		return *this;
	}
	uint64_t get_size(){return sizeof(T);}
	bool is_valid_pptr() const{
		return true;
	}
	bool is_null() const{
		return off == 0;
	}
};

template <class T>
class ptr_cnt{
public:
	T* ptr;//ptr with least 6 bits as counter
	ptr_cnt(T* p=nullptr, uint64_t cnt = 0) noexcept:
		ptr((T*)((uint64_t)p | (cnt & CACHELINE_MASK))){};
	void set(T* p, uint64_t cnt){
		assert(((uint64_t)p & CACHELINE_MASK) == 0);
		ptr = (T*)((uint64_t)p | (cnt & CACHELINE_MASK));
	}
	T* get_ptr() const{
		return (T*)((uint64_t)ptr & ~CACHELINE_MASK);
	}
	uint64_t get_counter() const{
		return (uint64_t)((uint64_t)ptr & CACHELINE_MASK);
	}
};

template <class T>
class atomic_pptr_cnt{//atomic pptr with 6 bits of counter
protected:
	atomic<int64_t> off;
public:
	atomic_pptr_cnt(T* v=nullptr, uint64_t counter=0)noexcept: //default constructor
		off(v==nullptr ? 0 : (int64_t)((uint64_t)v | (counter & CACHELINE_MASK)) - ((int64_t)this)) {};
	atomic_pptr_cnt(const pptr<T> &p, uint64_t counter=0)noexcept: //copy constructor
		off(p.is_null() ? 0 : (int64_t)((uint64_t)(p.off + (int64_t)&p) | (counter & CACHELINE_MASK)) - ((int64_t)this)) {};
	ptr_cnt<T> load(memory_order order = memory_order_seq_cst) const noexcept{
		int64_t cur_off = off.load(order);
		ptr_cnt<T> ret;
		ret.ptr = cur_off==0 ? nullptr : (T*)(cur_off + ((int64_t)this));
		return ret;
	}
	void store(ptr_cnt<T> desired, 
		memory_order order = memory_order_seq_cst ) noexcept{
		int64_t new_off = desired.get_ptr()==nullptr? 0 : ((int64_t)desired.ptr) - ((int64_t)this);
		off.store(new_off, order);
	}
	bool compare_exchange_weak(ptr_cnt<T>& expected, ptr_cnt<T> desired,
		memory_order order = memory_order_seq_cst ) noexcept{
		int64_t old_off = expected.get_ptr()==nullptr ? 0 : ((int64_t)expected.ptr) - ((int64_t)this);
		int64_t new_off = desired.get_ptr()==0 ? 0 : ((int64_t)(T*)desired.ptr) - ((int64_t)this);
		bool ret = off.compare_exchange_weak(old_off, new_off, order);
		if(!ret) {
			int64_t cur_off = off.load();
			if(cur_off == 0){
				expected.ptr = nullptr;
			} else{
				expected.ptr = (T*)(cur_off + ((int64_t)this));
			}
		}
		return ret;
	}
	bool compare_exchange_strong(ptr_cnt<T>& expected, ptr_cnt<T> desired,
		memory_order order = memory_order_seq_cst ) noexcept{
		int64_t old_off = expected.get_ptr()==nullptr ? 0 : ((int64_t)expected.ptr) - ((int64_t)this);
		int64_t new_off = desired.get_ptr()==0 ? 0 : ((int64_t)(T*)desired.ptr) - ((int64_t)this);
		bool ret = off.compare_exchange_strong(old_off, new_off, order);
		if(!ret) {
			int64_t cur_off = off.load();
			if(cur_off == 0){
				expected.ptr = nullptr;
			} else{
				expected.ptr = (T*)(cur_off + ((int64_t)this));
			}
		}
		return ret;
	}
};
template <class T>
inline bool operator==(const pptr<T>& lhs, const std::nullptr_t& rhs){
	return lhs.is_null();
}

template <class T>
inline bool operator==(const pptr<T>& lhs, const pptr<T>& rhs){
	return (T*)lhs == (T*)rhs;
}

template <class T>
inline bool operator!=(const pptr<T>& lhs, const std::nullptr_t& rhs){
	return !lhs.is_null();
}
// struct gc_ptr_base : public ptr_base{
// 	gc_ptr_base(void* v=nullptr):ptr_base(v){};
// 	virtual vector<gc_ptr_base*> filter_func(GarbageCollection* gc) {
// 		return {};
// 	}
// };

// template<class T>
// struct gc_ptr : public gc_ptr_base{
// 	gc_ptr(T* v=nullptr):gc_ptr_base((void*)v){};
// 	gc_ptr(pptr<T> v):gc_ptr_base((void*)v.val){};
// 	operator T*(){return (T*)val;};//cast to transient pointer
// 	T& operator *(){return *(T*)val;}//dereference
// 	T* operator ->(){return (T*)val;}//arrow
// 	vector<gc_ptr_base*> filter_func(GarbageCollection* gc){
// 		std::cout << "type name:" << typeid(T).name() << std::endl;
// 		return gc_ptr_base::filter_func(gc);
// 	}
// };

#endif