#ifndef _PPTR_HPP_
#define _PPTR_HPP_

#include <vector>
#include <iostream>
#include <cstddef>
#include "gc.hpp"
using namespace std;

class ptr_base{
protected:
	int64_t off;
public:
	// void* val;
	ptr_base(int64_t v=0){
		off = v;
	};
	
	// template<class T>
	// operator T*(){return static_cast<T*>(val);}//cast to transient pointer
};

template<class T>
class pptr : public ptr_base{
public:
	pptr(T* v=nullptr)noexcept: //default constructor
		ptr_base(((int64_t)v) - ((int64_t)this)) {};
	pptr(const pptr<T> &p)noexcept: //copy constructor
		ptr_base(((int64_t)(p.off + (int64_t)&p)) - ((int64_t)this)) {};
	pptr(const std::nullptr_t &p)noexcept: //copy constructor
		ptr_base(0) {};
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