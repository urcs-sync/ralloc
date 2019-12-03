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

// template<>
// vector<gc_ptr_base*> gc_ptr<Node>::filter_func(GarbageCollection* gc){
// 	std::cout << "type name:" << typeid(Node).name() << std::endl;
// 	vector<gc_ptr_base*> ret;
// 	ret.push_back(new gc_ptr<Node>((*this)->next));
// 	return ret;
// }

// struct container{
// 	gc_ptr_base* roots[3]={nullptr};
// 	function<vector<gc_ptr_base*>(gc_ptr_base*,GarbageCollection*)> filters[3];
// 	template<class T>
// 	void set_root(gc_ptr<T>* ptr, uint64_t i){
// 		roots[i] = ptr;
// 		// filters[i] = [](gc_ptr_base* ptr,GarbageCollection* gc){
// 		// 	gc_ptr<T> real_ptr(*ptr);
// 		// 	return real_ptr.filter_func(gc);
// 		// };
// 	}
// };

// void test_gc_ptr(){
// 	container* ctr;
// 	RegionManager* mgr;
// 	if(RegionManager::exists_test("/dev/shm/test")){
// 		cout<<"/dev/shm/test exist, testing remap...";
// 		mgr = new RegionManager("/dev/shm/test");
// 		void* hstart = mgr->__fetch_heap_start();
// 		ctr = (container*)hstart;
// 		ctr->roots[0]->filter_func(nullptr);
// 		ctr->roots[1]->filter_func(nullptr);
// 	} else {
// 		cout<<"create new RegionManager in /dev/shm/test...";
// 		mgr = new RegionManager("/dev/shm/test");
// 		bool res = mgr->__nvm_region_allocator((void**)&ctr,PAGESIZE,sizeof(container)+3*sizeof(Node)+3*sizeof(gc_ptr<Node>)); 
// 		if(!res) assert(0&&"mgr allocation fails!");
// 		mgr->__store_heap_start(ctr);
// 		new (ctr) container();
// 		void* nodes = ((uint64_t)ctr)+sizeof(container);
// 		Node* n1 = (Node*)nodes;
// 		new (n1) Node(1);
// 		Node2* n2 = (Node2*)(((uint64_t)nodes)+sizeof(Node));
// 		new (n2) Node2(2);

// 		gc_ptr<Node>* pn1 = (gc_ptr<Node>*)(((uint64_t)nodes)+3*sizeof(Node));
// 		new (pn1) gc_ptr<Node>(n1);
// 		gc_ptr<Node2>* pn2 = (gc_ptr<Node2>*)(((uint64_t)nodes)+3*sizeof(Node)+sizeof(gc_ptr<Node>));
// 		new (pn2) gc_ptr<Node2>(n2);

// 		ctr->set_root(pn1, 0);
// 		ctr->set_root(pn2, 1);
// 	}
// 	delete mgr;
// 	cout<<"done!\n";
// }

void test_pptr(){
	RegionManager* mgr;
	if(RegionManager::exists_test("/dev/shm/test")){
		cout<<"/dev/shm/test exist, testing remap...";
		mgr = new RegionManager("/dev/shm/test");
		void* hstart = mgr->__fetch_heap_start();
		pptr<Node>* p1 = (pptr<Node>*)hstart;
		pptr<Node2>* p2 = (pptr<Node2>*)(((uint64_t)p1)+2*sizeof(Node)+sizeof(pptr<Node>));
		pptr<Node>* p3 = (pptr<Node>*)(((uint64_t)p1)+2*sizeof(Node)+2*sizeof(pptr<Node>));
		Node* tp1 = (Node*)(*p1);
		cout<<"tp1: "<<tp1->val<<" = 7"<<endl;
		*p1 = tp1;
		cout<<"p1: "<<(*p1)->val<<" = 7"<<endl;
		cout<<"p2: "<<(**p2).val<<" = 13"<<endl;
		cout<<"p3: "<<(*p3)->val<<" = 7"<<endl;
		*p3 = *p1;
		cout<<"p3: "<<(*p3)->val<<" = 7"<<endl;
		assert((*p1) == (*p3));
		atomic<pptr<Node>> atm_p = *p1;
		cout<<"atm_p: "<<atm_p.load()->val<<" = 7"<<endl;
		atm_p.store(*p3);
		cout<<"atm_p: "<<atm_p.load()->val<<" = 7"<<endl;
		atm_p.compare_exchange_strong(*p3,*p1);
		cout<<"atm_p: "<<atm_p.load()->val<<" = 7"<<endl;
	} else {
		cout<<"create new RegionManager in /dev/shm/test...";
		mgr = new RegionManager("/dev/shm/test");
		char * start;
		bool res = mgr->__nvm_region_allocator((void**)&start,PAGESIZE,2*sizeof(Node)+3*sizeof(pptr<Node>)); 
		if(!res) assert(0&&"mgr allocation fails!");
		mgr->__store_heap_start(start);
		/* Layout:
			pptr<Node>
			Node
			Node2
			pptr<Node2>
			pptr<Node>
		*/
		new (start + sizeof(pptr<Node>)) Node(7);
		new (start + sizeof(pptr<Node>) + sizeof(Node)) Node2(13);
		new (start) pptr<Node>((Node*)(start + sizeof(pptr<Node>)));
		new (start + sizeof(pptr<Node>) + 2*sizeof(Node)) pptr<Node2>((Node2*)(start + sizeof(pptr<Node>) + sizeof(Node)));
		new (start + 2*sizeof(pptr<Node>) + 2*sizeof(Node)) pptr<Node>(*(pptr<Node>*)start);
	}
	delete mgr;
	cout<<"done!\n";
}

int main(){
	test_pptr();
}