#include <vector>
using namespace std;

class pptr_base{
protected:
// 	uint64_t off;
	void* val;
public:
	pptr_base(void* v=nullptr):val(v){};
	
	template<class T>
	operator T*(){return static_cast<T*>(val);}//cast to transient pointer
	virtual vector<pptr_base*> filter_func(){
		return {};
	}
	virtual uint64_t get_size(){return 0;}
};
template<class T>
class pptr : public pptr_base{
public:
	pptr(T* v=nullptr):pptr_base((void*)v){};
	// operator (T*)();//cast to transient pointer
	T& operator *(){return *(T*)val;}//dereference
	T* operator ->(){return (T*)val;}//arrow
	// pptr():off(0){};
	// pptr(uint64_t o = 0):off(o){};
	// pptr(T v, uint64_t o = 0):off(o),val(v){};
	uint64_t get_size(){return sizeof(T);}
	virtual vector<pptr_base*> filter_func(){return pptr_base::filter_func();};
	// vector<pptr_base*> filter_func() __attribute__((weak));
};
