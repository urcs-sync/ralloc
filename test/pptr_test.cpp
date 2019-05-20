#include <cstdio>
#include <iostream>
#include <memory>
#include <functional>
#include <vector>
#include "pptr.hpp"
#include "RegionManager.hpp"
using namespace std;

struct Node{
	int val;
	pptr<Node> next;
	Node(int v = 0, Node* n = nullptr):
		val(v),next(n){};
};

struct Node2{
	int val;
	pptr<Node2> next;
	Node2(int v = 0, Node2* n = nullptr):
		val(v),next(n){};
};

template<>
vector<gc_ptr_base*> gc_ptr<Node>::filter_func(GarbageCollection* gc){
	std::cout << "type name:" << typeid(Node).name() << std::endl;
	vector<gc_ptr_base*> ret;
	ret.push_back(new gc_ptr<Node>((*this)->next));
	return ret;
}

struct container{
	gc_ptr_base* roots[3]={nullptr};
	function<vector<gc_ptr_base*>(gc_ptr_base*,GarbageCollection*)> filters[3];
	template<class T>
	void set_root(gc_ptr<T>* ptr, uint64_t i){
		roots[i] = ptr;
		// filters[i] = [](gc_ptr_base* ptr,GarbageCollection* gc){
		// 	gc_ptr<T> real_ptr(*ptr);
		// 	return real_ptr.filter_func(gc);
		// };
	}
};
//todo: test if vtable still exists while restarting.
int main(){
	// pptr<Node> a( new Node(1) );
	// a->next = new Node(2);
	// a->next->next = new Node(3);
	// auto content = gc_ptr(a).filter_func();
	// auto content2 = content[0]->filter_func();
	// gc_ptr_base* toread = content2[0];
	// cout<<(*(dynamic_cast<gc_ptr<Node>*>(toread)))->val<<" and it should be 3"<<endl;
	// pptr<Node2> aa(new Node2(4));
	// cout<<aa->val<<" and it should be 4\n";
	// auto content3 = gc_ptr(aa).filter_func();
	// cout<<content3.size()<<" and the size should be 0\n";
	container* ctr;
	RegionManager* mgr;
	if(RegionManager::exists_test("/dev/shm/test")){
		cout<<"/dev/shm/test exist, testing remap...";
		mgr = new RegionManager("/dev/shm/test");
		void* hstart = mgr->__fetch_heap_start();
		ctr = (container*)hstart;
		ctr->roots[0]->filter_func(nullptr);
		ctr->roots[1]->filter_func(nullptr);
	} else {
		cout<<"create new RegionManager in /dev/shm/test...";
		mgr = new RegionManager("/dev/shm/test");
		bool res = mgr->__nvm_region_allocator((void**)&ctr,PAGESIZE,sizeof(container)+3*sizeof(Node)+3*sizeof(gc_ptr<Node>)); 
		if(!res) assert(0&&"mgr allocation fails!");
		mgr->__store_heap_start(ctr);
		new (ctr) container();
		void* nodes = ctr+sizeof(container);
		Node* n1 = (Node*)nodes;
		new (n1) Node(1);
		Node2* n2 = (Node2*)((uint64_t)nodes+sizeof(Node));
		new (n2) Node2(2);

		gc_ptr<Node>* pn1 = (gc_ptr<Node>*)((uint64_t)nodes+3*sizeof(Node));
		new (pn1) gc_ptr<Node>(n1);
		gc_ptr<Node2>* pn2 = (gc_ptr<Node2>*)((uint64_t)nodes+3*sizeof(Node)+sizeof(gc_ptr<Node>));
		new (pn2) gc_ptr<Node2>(n2);

		ctr->set_root(pn1, 0);
		ctr->set_root(pn2, 1);
	}
	delete mgr;
	cout<<"done!\n";
}