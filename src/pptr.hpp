#include <vector>
using namespace std;

class pptr_base{
private:
	uint64_t off;
public:
	virtual pptr_base* get_self() = 0;
	virtual vector<pptr_base*> filter_func(){
		
	}
};
template<class T>
class pptr : public pptr_base{

public:
	// T val;
	pptr<T>* get_self(){return this;}
	// operator (T*)();//cast to transient pointer
	// T& operator *();//dereference
	// T* operator ->();//arrow
	pptr():off(0){};
	pptr(uint64_t o = 0):off(o){};
	pptr(T v, uint64_t o = 0):off(o),val(v){};
	// vector<pptr_base*> filter_func() __attribute__((weak));
};