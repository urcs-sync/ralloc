#ifndef _PPTR_HPP_
#define _PPTR_HPP_

#include <vector>
#include <iostream>
#include <cstddef>
#include <atomic>
#include "pm_config.hpp"
using namespace std;

/*
 * Class pptr is a templated class implemented off-holder. See paper:
 * Efficient Support of Position Independence on Non-Volatile Memory
 * 		Guoyang Chen et al., MICRO'2017
 *
 * It stores the offset from the instance itself to the object it points to.
 * The offset can be negative.
 * 
 * Two kinds of constructors and casting to transient pointer are provided,
 * as well as dereference, arrow access, assignment, and comparison.
 */
template<class T>
class pptr{
public:
	int64_t off;
	pptr(T* v=nullptr)noexcept: //default constructor
		off(v==nullptr ? 0 : ((int64_t)v) - ((int64_t)this)) {};
	pptr(const pptr<T> &p)noexcept: //copy constructor
		off(p.is_null() ? 0 : ((int64_t)(p.off + (int64_t)&p)) - ((int64_t)this)) {};
	template<class F>
	inline operator F*() const{ //cast to transient pointer
		return off==0 ? nullptr : (F*)(off + ((int64_t)this));
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
	template<class F>
	inline pptr& operator = (const F* p){ //assignment
		if(p == nullptr) {
			off = 0;
		} else {
			off = ((int64_t)p) - ((int64_t)this);
		}
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

template <class T>
inline bool operator!=(const pptr<T>& lhs, const pptr<T>& rhs){
	return !((T*)lhs == (T*)rhs);
}

/* 
 * Class ptr_cnt is also a templated class, but is a wrapper of plain pointer.
 * The least significant 6 bits are for ABA counter, and the user should 
 * guarantee the thing we points to is always aligned to 64 byte (expected to 
 * be the cache line  size).
 * 
 * This class is mainly to store the intermediate value of operations on 
 * atomic_pptr_cnt<T> defined below, including atomic load, store, and CAS.
 */
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

/* 
 * Class atomic_pptr_cnt is considered the atomic and ABA-counter-added 
 * version of pptr.It's initialized by a pointer and a counter.
 *
 * The member *off* stores the offset from the instance of atomic_pptr_cnt to 
 * the object it points to PLUS the ABA counter. As a result, when you add off
 * to pointer *this*, you'll get something like ptr_cnt.
 *
 * It defines load, store, compare_exchange_weak, and compare_exchange_strong
 * with the same specification of atomic, but returns and/or takes desired and 
 * expected value in type of ptr_cnt<T>.
 */
template <class T>
class atomic_pptr_cnt{//atomic pptr with 6 bits of counter
public:
	atomic<int64_t> off;
	atomic_pptr_cnt(T* v=nullptr, uint64_t counter=0)noexcept: //default constructor
		off(v==nullptr ? 0 : (((uint64_t)v | (counter & CACHELINE_MASK)) - ((uint64_t)this))) {};
	inline ptr_cnt<T> load(memory_order order = memory_order_seq_cst) const noexcept{
		int64_t cur_off = off.load(order);
		ptr_cnt<T> ret;
		if(cur_off >=0 && cur_off<CACHELINE_SIZE){
			ret.ptr = (T*)cur_off;
		} else{
			 ret.ptr = (T*)(cur_off + ((uint64_t)this));
		}
		return ret;
	}
	inline void store(ptr_cnt<T> desired, 
		memory_order order = memory_order_seq_cst ) noexcept{
		int64_t new_off;
		if (desired.get_ptr() == nullptr){
			new_off = desired.get_counter();
		} else {
			new_off = (uint64_t)desired.ptr - (uint64_t)this;
		}
		off.store(new_off, order);
	}
	inline bool compare_exchange_weak(ptr_cnt<T>& expected, ptr_cnt<T> desired,
		memory_order order = memory_order_seq_cst ) noexcept{
		int64_t old_off, new_off;
		if(expected.get_ptr()==nullptr){
			old_off = expected.get_counter();
		} else {
			old_off = (uint64_t)expected.ptr - (uint64_t)this;
		}
		if(desired.get_ptr()==nullptr){
			new_off = desired.get_counter();
		} else {
			new_off = (uint64_t)desired.ptr - (uint64_t)this;
		}
		bool ret = off.compare_exchange_weak(old_off, new_off, order);
		if(!ret) {
			if(old_off >= 0 && old_off < CACHELINE_SIZE){
				expected.ptr = (T*)old_off;
			} else{
				expected.ptr = (T*)(old_off + (uint64_t)this);
			}
		}
		return ret;
	}
	inline bool compare_exchange_strong(ptr_cnt<T>& expected, ptr_cnt<T> desired,
		memory_order order = memory_order_seq_cst ) noexcept{
		int64_t old_off, new_off;
		if(expected.get_ptr()==nullptr){
			old_off = expected.get_counter();
		} else {
			old_off = (uint64_t)expected.ptr - (uint64_t)this;
		}
		if(desired.get_ptr()==nullptr){
			new_off = desired.get_counter();
		} else {
			new_off = (uint64_t)desired.ptr - (uint64_t)this;
		}
		bool ret = off.compare_exchange_strong(old_off, new_off, order);
		if(!ret) {
			if(old_off >= 0 && old_off < CACHELINE_SIZE){
				expected.ptr = (T*)old_off;
			} else{
				expected.ptr = (T*)(old_off + (uint64_t)this);
			}
		}
		return ret;
	}
};

/* 
 * Class atomic_pptr is considered the atomic version of pptr.
 * It's initialized by a pointer or pptr<T>.
 *
 * The member *off* stores the offset from the instance of atomic_pptr to 
 * the object it points to.
 *
 * It defines load, store, compare_exchange_weak, and compare_exchange_strong
 * with the same specification of atomic, but returns and/or takes desired and 
 * expected value in type of T*.
 */
template <class T> 
class atomic_pptr{
public:
	atomic<int64_t> off;
	atomic_pptr(T* v=nullptr)noexcept: //default constructor
		off(v==nullptr ? 0 : ((int64_t)v) - ((int64_t)this)) {};
	atomic_pptr(const pptr<T> &p)noexcept: //copy constructor
		off(p.is_null() ? 0 : (int64_t)(p.off + (int64_t)&p) - ((int64_t)this)) {};
	inline atomic_pptr& operator = (const atomic_pptr &p){ //assignment
		off.store(((int64_t)(p.off.load() + (int64_t)&p)) - ((int64_t)this));
		return *this;
	}
	template<class F>
	inline atomic_pptr& operator = (const F* p){ //assignment
		if(p == nullptr) {
			off.store(0);
		} else {
			off.store(((int64_t)p) - ((int64_t)this));
		}
		return *this;
	}
	T* load(memory_order order = memory_order_seq_cst) const noexcept{
		int64_t cur_off = off.load(order);
		T* ret;
		ret = cur_off==0 ? nullptr : (T*)(cur_off + ((int64_t)this));
		return ret;
	}
	void store(T* desired, 
		memory_order order = memory_order_seq_cst ) noexcept{
		int64_t new_off = desired==nullptr? 0 : ((int64_t)desired) - ((int64_t)this);
		off.store(new_off, order);
	}
	bool compare_exchange_weak(T*& expected, T* desired,
		memory_order order = memory_order_seq_cst ) noexcept{
		int64_t old_off = expected==nullptr ? 0 : ((int64_t)expected) - ((int64_t)this);
		int64_t new_off = desired==nullptr ? 0 : ((int64_t)desired) - ((int64_t)this);
		bool ret = off.compare_exchange_weak(old_off, new_off, order);
		if(!ret) {
			if(old_off == 0){
				expected = nullptr;
			} else{
				expected = (T*)(old_off + ((int64_t)this));
			}
		}
		return ret;
	}
	bool compare_exchange_strong(T*& expected, T* desired,
		memory_order order = memory_order_seq_cst ) noexcept{
		int64_t old_off = expected==nullptr ? 0 : ((int64_t)expected) - ((int64_t)this);
		int64_t new_off = desired==nullptr ? 0 : ((int64_t)desired) - ((int64_t)this);
		bool ret = off.compare_exchange_strong(old_off, new_off, order);
		if(!ret) {
			if(old_off == 0){
				expected = nullptr;
			} else{
				expected = (T*)(old_off + ((int64_t)this));
			}
		}
		return ret;
	}
};

#endif