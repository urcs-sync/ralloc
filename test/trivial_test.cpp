#include <iostream>
#include <atomic>
using namespace std;
struct _test{
	int a[10];
};

struct _16atomic{
	uint32_t index;
	uint64_t value;
	uint32_t counter;
	_16atomic(uint32_t a=0, uint64_t b=0, uint32_t c=0):
		index(a),
		value(b),
		counter(c){};
};

struct _12atomic{
	uint64_t value;
	uint32_t counter;
	_12atomic(uint64_t b=0, uint32_t c=0):
		value(b),
		counter(c){};
};

int main() {
	cout<<"size of _test: "<<sizeof(_test)<<endl;
	_test x;
	cout<<"address of x: "<<&x<<endl;
	cout<<"address of x.a[0]: "<<&x.a[0]<<endl;
	cout<<"address of x.a[9]: "<<&x.a[9]<<endl;
	cout<<"address of x.a: "<<&x.a<<endl;
	// _16atomic tmp{0,10,0};
	atomic<_12atomic> t{{0,0}};
	_12atomic new_val{10,0};
	auto old_val = t.load();
	cout<<"previous val: " << (uint64_t)t.load().value<<endl;
	t.compare_exchange_strong(old_val,new_val);
	cout<<"current val: " << (uint64_t)t.load().value<<endl;
}