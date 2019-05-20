#ifndef _PPTR_HPP_
#define _PPTR_HPP_

#include <vector>
#include <iostream>
#include "gc.hpp"
using namespace std;

class ptr_base{
// protected:
// 	uint64_t off;
public:
	void* val;
	ptr_base(void* v=nullptr):val(v){};
	
	template<class T>
	operator T*(){return static_cast<T*>(val);}//cast to transient pointer
};

template<class T>
class pptr : public ptr_base{
public:
	pptr(T* v=nullptr):ptr_base((void*)v){};
	operator T*(){return (T*)val;}//cast to transient pointer
	T& operator *(){return *(T*)val;}//dereference
	T* operator ->(){return (T*)val;}//arrow
	uint64_t get_size(){return sizeof(T);}
	bool is_valid_pptr() const{
		return true;
	}
};

struct gc_ptr_base : public ptr_base{
	gc_ptr_base(void* v=nullptr):ptr_base(v){};
	virtual vector<gc_ptr_base*> filter_func(GarbageCollection* gc) {
		return {};
	}
};

template<class T>
struct gc_ptr : public gc_ptr_base{
	gc_ptr(T* v=nullptr):gc_ptr_base((void*)v){};
	gc_ptr(pptr<T> v):gc_ptr_base((void*)v.val){};
	operator T*(){return (T*)val;};//cast to transient pointer
	T& operator *(){return *(T*)val;}//dereference
	T* operator ->(){return (T*)val;}//arrow
	vector<gc_ptr_base*> filter_func(GarbageCollection* gc){
		std::cout << "type name:" << typeid(T).name() << std::endl;
		return gc_ptr_base::filter_func(gc);
	}
};

#endif