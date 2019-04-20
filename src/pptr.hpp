#include <vector>
using namespace std;

class ptr_base{
// protected:
// 	uint64_t off;
public:
	void* val;
	ptr_base(void* v=nullptr):val(v){};
	
	template<class T>
	operator T*(){return static_cast<T*>(val);}//cast to transient pointer
	// virtual vector<ptr_base*> filter_func(){
		// return {};
	// }
	// virtual uint64_t get_size(){return 0;}
};

// class gc_ptr_base : public ptr_base;
// template<class T>
// class gc_ptr : public gc_ptr_base;

template<class T>
class pptr : public ptr_base{
public:
	pptr(T* v=nullptr):ptr_base((void*)v){};
	// operator (T*)();//cast to transient pointer
	T& operator *(){return *(T*)val;}//dereference
	T* operator ->(){return (T*)val;}//arrow
	// pptr():off(0){};
	// pptr(uint64_t o = 0):off(o){};
	// pptr(T v, uint64_t o = 0):off(o),val(v){};
	uint64_t get_size(){return sizeof(T);}
	// virtual vector<ptr_base*> filter_func(){return ptr_base::filter_func();};
	// vector<ptr_base*> filter_func() __attribute__((weak));
};

struct gc_ptr_base : public ptr_base{
	bool marked = false;
	gc_ptr_base(void* v=nullptr):ptr_base(v){};
	void mark(gc_ptr_base* p){
		//todo: check ptr is valid 
		if(!marked){
			marked = true;
			p->filter_func();
		}
	}
	virtual void filter_func() {return {};}
};

template<class T>
struct gc_ptr : public gc_ptr_base{
	T& operator *(){return *(T*)val;}//dereference
	T* operator ->(){return (T*)val;}//arrow
	gc_ptr(T* v=nullptr):gc_ptr_base((void*)v){};
	gc_ptr(pptr<T> v):gc_ptr_base((void*)v.val){};
	template <class F = mark_func>
	vector<gc_ptr_base*> filter_func(){
		return gc_ptr_base::filter_func();
	}
};
