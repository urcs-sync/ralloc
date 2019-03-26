#include <cstdio>
#include <iostream>
#include <memory>
#include "pptr.hpp"
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
vector<gc_ptr_base*> gc_ptr<Node>::filter_func(){
	vector<gc_ptr_base*> ret;
	ret.push_back(new gc_ptr<Node>((*this)->next));
	return ret;
}

int main(){
	pptr<Node> a( new Node(1) );
	a->next = new Node(2);
	a->next->next = new Node(3);
	auto content = gc_ptr(a).filter_func();
	auto content2 = content[0]->filter_func();
	gc_ptr_base* toread = content2[0];
	cout<<(*(dynamic_cast<gc_ptr<Node>*>(toread)))->val<<" and it should be 3"<<endl;
	pptr<Node2> aa(new Node2(4));
	cout<<aa->val<<" and it should be 4\n";
	auto content3 = gc_ptr(aa).filter_func();
	cout<<content3.size()<<" and the size should be 0\n";
}