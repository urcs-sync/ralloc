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

inline bool is_null_pptr(uint64_t off) {
	return off == PPTR_PATTERN_POS;
}

inline bool is_valid_pptr(uint64_t off) {
	return (off & PPTR_PATTERN_MASK) == PPTR_PATTERN_POS;
}
template<class T>
class pptr;
template<class T>
class atomic_pptr;

template<class T>
inline uint64_t to_pptr_off(const T* v, const pptr<T>* p) {
	uint64_t off;
	if(v == nullptr) {
		off = PPTR_PATTERN_POS;
	} else {
		if(v > reinterpret_cast<const T*>(p)) {
			off = ((uint64_t)v) - ((uint64_t)p);
			off = off << PPTR_PATTERN_SHIFT;
			off = off | PPTR_PATTERN_POS;
		} else {
			off = ((uint64_t)p) - ((uint64_t)v);
			off = off << PPTR_PATTERN_SHIFT;
			off = off | PPTR_PATTERN_NEG;
		}
	}
	return off;
}

template<class T>
inline T* from_pptr_off(uint64_t off, const pptr<T>* p) {
	if(!is_valid_pptr(off) || is_null_pptr(off)) { 
		return nullptr;
	} else {
		if(off & 1) { // sign bit is true (negative)
			return (T*)(((int64_t)p) - (off>>PPTR_PATTERN_SHIFT));
		} else {
			return (T*)(((int64_t)p) + (off>>PPTR_PATTERN_SHIFT));
		}
	}
}

template<class T>
inline uint64_t to_pptr_off(const T* v, const atomic_pptr<T>* p) {
	return to_pptr_off(v, reinterpret_cast<const pptr<T>*>(p));
}

template<class T>
inline T* from_pptr_off(uint64_t off, const atomic_pptr<T>* p) {
	return from_pptr_off(off, reinterpret_cast<const pptr<T>*>(p));
}

template<class T>
class pptr{
public:
	uint64_t off;
	pptr(T* v=nullptr) noexcept { //default constructor
		off = to_pptr_off(v, this);
	};
	pptr(const pptr<T> &p) noexcept { //copy constructor
		T* v = static_cast<T*>(p);
		off = to_pptr_off(v, this);
	}

	template<class F>
	inline operator F*() const{ //cast to transient pointer
		return from_pptr_off(off, this);
	}
	inline T& operator * () { //dereference
		return *static_cast<T*>(*this);
	}
	inline T* operator -> (){ //arrow
		return static_cast<T*>(*this);
	}
	template<class F>
	inline pptr& operator = (const F* v){ //assignment
		off = to_pptr_off(v, this);
		return *this;
	}
	inline pptr& operator = (const pptr &p){ //assignment
		T* v = static_cast<T*>(p);
		off = to_pptr_off(v, this);
		return *this;
	}
	inline T& operator [] (size_t idx) const { // subscript
	        return static_cast<T*>(*this)[idx];
	}
	bool is_null() const {
		return off == PPTR_PATTERN_POS;
	}

	bool is_valid() const {
		return (off & PPTR_PATTERN_MASK) == PPTR_PATTERN_POS;
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
	atomic<uint64_t> off;
	atomic_pptr(T* v=nullptr) noexcept { //default constructor
		uint64_t tmp_off = to_pptr_off(v, this);
		off.store(tmp_off);
	}
	atomic_pptr(const pptr<T> &p) noexcept { //copy constructor
		T* v = static_cast<T*>(p);
		uint64_t tmp_off = to_pptr_off(v, this);
		off.store(tmp_off);
	}
	inline atomic_pptr& operator = (const atomic_pptr &p){ //assignment
		T* v = p.load();
		uint64_t tmp_off = to_pptr_off(v, this);
		off.store(tmp_off);
		return *this;
	}
	template<class F>
	inline atomic_pptr& operator = (const F* v){ //assignment
		uint64_t tmp_off = to_pptr_off(v, this);
		off.store(tmp_off);
		return *this;
	}
	T* load(memory_order order = memory_order_seq_cst) const noexcept{
		uint64_t cur_off = off.load(order);
		return from_pptr_off(cur_off, this);
	}
	void store(T* desired, 
		memory_order order = memory_order_seq_cst ) noexcept{
		uint64_t new_off = to_pptr_off(desired, this);
		off.store(new_off, order);
	}
	bool compare_exchange_weak(T*& expected, T* desired,
		memory_order order = memory_order_seq_cst ) noexcept{
		uint64_t old_off = to_pptr_off(expected, this);
		uint64_t new_off = to_pptr_off(desired, this);
		bool ret = off.compare_exchange_weak(old_off, new_off, order);
		if(!ret) {
			if(is_null_pptr(old_off)){
				expected = nullptr;
			} else{
				expected = from_pptr_off(old_off, this);
			}
		}
		return ret;
	}
	bool compare_exchange_strong(T*& expected, T* desired,
		memory_order order = memory_order_seq_cst ) noexcept{
		uint64_t old_off = to_pptr_off(expected, this);
		uint64_t new_off = to_pptr_off(desired, this);
		bool ret = off.compare_exchange_strong(old_off, new_off, order);
		if(!ret) {
			if(is_null_pptr(old_off)){
				expected = nullptr;
			} else{
				expected = from_pptr_off(old_off, this);
			}
		}
		return ret;
	}
};

/* 
 * Class ptr_cnt is a templated class, but is a wrapper of plain pointer.
 * The least significant 6 bits are for ABA counter, and the user should 
 * guarantee the thing we points to is always aligned to 64 byte (expected to 
 * be the cache line  size).
 * 
 * This class is mainly to store the intermediate value of operations on 
 * AtomicCrossPtrCnt<T> defined in BaseMeta.hpp, including atomic load, store, 
 * and CAS.
 */
template <class T>
class ptr_cnt{
public:
	T* ptr;//ptr with least 6 bits as counter
	uint64_t cnt;
	ptr_cnt(T* p=nullptr, uint64_t c = 0) noexcept:
		ptr(p), cnt(c){};
	void set(T* p, uint64_t c){
		ptr = p;
		cnt = c;
	}
	T* get_ptr() const{
		return ptr;
	}
	uint64_t get_counter() const{
		return cnt;
	}
};
#endif
