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

template<class T>
struct _12atomic{
	T value;
	uint32_t counter;
	_12atomic(T b=0, uint32_t c=0) noexcept:
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
	atomic<_12atomic<uint64_t>> t[10];
	_12atomic<uint64_t> new_val{~(uint64_t)0,464564353};
	cout<<"sizeof 12atomic<uint64_t>: "<<sizeof(_12atomic<uint64_t>)<<endl;
	cout<<"sizeof 12atomic<uint32_t>: "<<sizeof(_12atomic<uint32_t>)<<endl;
	cout<<"new val: " <<new_val.value<<endl;
	cout<<"new cnt: " <<new_val.counter<<endl;
	for(int i=0;i<10;i++){
		auto old_val = t[i].load();
		cout<<"previous t["<<i<<"]: "<< (uint64_t)t[i].load().value<<endl;
fuck:		t[i].compare_exchange_strong(old_val,new_val);
		for(int j=0;j<=i;j++){
			// cout<<"current t["<<j<<"].val: " <<t[j].load().value<<endl;
			// cout<<"current t["<<j<<"].cnt: " <<t[j].load().counter<<endl;
		}
	}
}