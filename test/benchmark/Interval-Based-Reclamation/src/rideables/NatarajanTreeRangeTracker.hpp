/*

Copyright 2017 University of Rochester

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. 

*/


#ifndef NATARAJAN_TREE_RANGETRACKER
#define NATARAJAN_TREE_RANGETRACKER

#include <iostream>
#include <atomic>
#include <algorithm>
#include "Harness.hpp"
#include "ConcurrentPrimitives.hpp"
#include "ROrderedMap.hpp"
#include "HazardTracker.hpp"
#include "RUnorderedMap.hpp"
// #include "ssmem.h"

#include "RangeTracker.hpp"

// #include "RangeTracker.hpp"


#include "RetiredMonitorable.hpp"

// //GC Method: ssmem from LPD-EPFL
// #ifdef NGC
// #define ssmem_alloc(x,y) malloc(y)
// #define ssmem_free(x,y)
// #endif
#ifdef NGC
#define COLLECT false
#else
#define COLLECT true
#endif

template <class K, class V>
class NatarajanTreeRangeTracker : public ROrderedMap<K,V>, public RetiredMonitorable{
private:
	/* structs*/
	struct Node{
		int level;
		K key;
		V val;
		biptr<Node> left;
		biptr<Node> right;

		virtual ~Node(){};
		static Node* alloc(K k, V v, Node* l, Node* r,RangeTracker<Node>* range_tracker, int tid){//TODO: Switch this func to RangeTracker one
			return alloc(k,v,l,r,-1,range_tracker,tid);
		}

		static Node* alloc(K k, V v, Node* l, Node* r, int lev, RangeTracker<Node>* range_tracker, int tid){
			while(true){
				Node* n = (Node*) range_tracker->alloc(tid);
				if (n==nullptr) {
					// std::cout<<"alloc fails\n";
					continue;//return alloc(a,k,v,l,r,lev);
				}
				//don't know how to call constructor for malloc
				//http://stackoverflow.com/questions/2995099/malloc-and-constructors
				new (n) Node(k,v,l,r,lev);
				return n;
			}
		}
		Node(K k, V v, Node* l, Node* r,int lev):level(lev),key(k),val(v),left(l),right(r){};
		Node(K k, V v, Node* l, Node* r):level(-1),key(k),val(v),left(l),right(r){};
	};
	struct SeekRecord{
		Node* ancestor;
		Node* successor;
		Node* parent;
		Node* leaf;
	};

	/* variables */
	RangeTracker<Node>* range_tracker;
	K infK{};
	V defltV{};
	// Node r{infK,defltV,nullptr,nullptr,2};
	// Node s{infK,defltV,nullptr,nullptr,1};
	Node* r;
	Node* s;

	padded<SeekRecord>* records;
	const size_t GET_POINTER_BITS = 0xfffffffffffffffc;//for machine 64-bit or less.

	/* helper functions */
	//flag and tags helpers
	inline Node* getPtr(Node* mptr){
		return (Node*) ((size_t)mptr & GET_POINTER_BITS);
	}
	inline bool getFlg(Node* mptr){
		return (bool)((size_t)mptr & 1);
	}
	inline bool getTg(Node* mptr){
		return (bool)((size_t)mptr & 2);
	}
	inline Node* mixPtrFlgTg(Node* ptr, bool flg, bool tg){
		return (Node*) ((size_t)ptr | flg | ((size_t)tg<<1));
	}
	//node comparison
	inline bool isInf(Node* n){
		return getInfLevel(n)!=-1;
	}
	inline int getInfLevel(Node* n){
		//0 for inf0, 1 for inf1, 2 for inf2, -1 for general val
		n=getPtr(n);
		return n->level;
	}
	inline bool nodeLess(Node* n1, Node* n2){
		n1=getPtr(n1);
		n2=getPtr(n2);
		int i1=getInfLevel(n1);
		int i2=getInfLevel(n2);
		return i1<i2 || (i1==-1&&i2==-1&&n1->key<n2->key);
	}
	inline bool nodeEqual(Node* n1, Node* n2){
		n1=getPtr(n1);
		n2=getPtr(n2);
		int i1=getInfLevel(n1);
		int i2=getInfLevel(n2);
		if(i1==-1&&i2==-1)
			return n1->key==n2->key;
		else
			return i1==i2;
	}
	inline bool nodeLessEqual(Node* n1, Node* n2){
		return !nodeLess(n2,n1);
	}

	/* private interfaces */
	void seek(K key, int tid);
	bool cleanup(K key, int tid);
	void doRangeQuery(Node& k1, Node& k2, int tid, Node* root, std::map<K,V>& res);
public:
	NatarajanTreeRangeTracker(GlobalTestConfig* gtc): RetiredMonitorable(gtc)
	{//TODO: finish range_tracker initialization.
		int epochf = gtc->getEnv("epochf").empty()? 150:stoi(gtc->getEnv("epochf"));
		int emptyf = gtc->getEnv("emptyf").empty()? 30:stoi(gtc->getEnv("emptyf"));
		range_tracker = new RangeTracker<Node>(gtc,epochf,emptyf,true);
		r = Node::alloc(infK,defltV,nullptr,nullptr,2,range_tracker,0);
		s = Node::alloc(infK,defltV,nullptr,nullptr,1,range_tracker,0);
		r->right = Node::alloc(infK,defltV,nullptr,nullptr,2,range_tracker,0);
		r->left = s;
		s->right = Node::alloc(infK,defltV,nullptr,nullptr,1,range_tracker,0);
		s->left = Node::alloc(infK,defltV,nullptr,nullptr,0,range_tracker,0);
		records = new padded<SeekRecord>[gtc->task_num]{};
	};
	~NatarajanTreeRangeTracker(){};

	optional<V> get(K key, int tid);
	optional<V> put(K key, V val, int tid);
	bool insert(K key, V val, int tid);
	optional<V> remove(K key, int tid);
	optional<V> replace(K key, V val, int tid);
	std::map<K, V> rangeQuery(K key1, K key2, int& len, int tid);
};

template <class K, class V> 
class NatarajanTreeRangeTrackerFactory : public RideableFactory{
	NatarajanTreeRangeTracker<K,V>* build(GlobalTestConfig* gtc){
		return new NatarajanTreeRangeTracker<K,V>(gtc);
	}
};

//-------Definition----------
template <class K, class V>
void NatarajanTreeRangeTracker<K,V>::seek(K key, int tid){
	Node keyNode{key,defltV,nullptr,nullptr};//node to be compared
	SeekRecord* seekRecord=&(records[tid].ui);
	seekRecord->ancestor=r;
	seekRecord->successor=range_tracker->read(r->left);
	seekRecord->parent=range_tracker->read(r->left);
	seekRecord->leaf=getPtr(range_tracker->read(s->left));

	/* initialize other variables used in the traversal */
	Node* parentField=range_tracker->read(seekRecord->parent->left);
	Node* currentField=range_tracker->read(seekRecord->leaf->left);
	Node* current=getPtr(currentField);

	/* traverse the tree */
	while(current!=nullptr){
		/* check if the edge from the current parent node is tagged */
		if(!getTg(parentField)){
			/* 
			 * found an untagged edge in the access path;
			 * advance ancestor and successor pointers.
			 */
			seekRecord->ancestor=seekRecord->parent;
			// range_tracker->transfer(1,0,tid);
			seekRecord->successor=seekRecord->leaf;
			// range_tracker->transfer(3,1,tid);
		}

		/* advance parent and leaf pointers */
		seekRecord->parent=seekRecord->leaf;
		// range_tracker->transfer(3,2,tid);
		seekRecord->leaf=current;
		// range_tracker->transfer(4,3,tid);

		/* update other variables used in traversal */
		parentField=currentField;
		if(nodeLess(&keyNode,current)){
			currentField=range_tracker->read(current->left);
		}
		else{
			currentField=range_tracker->read(current->right);
		}
		current=getPtr(currentField);
	}
	return;
}

template <class K, class V>
bool NatarajanTreeRangeTracker<K,V>::cleanup(K key, int tid){
	Node keyNode{key,defltV,nullptr,nullptr};//node to be compared
	bool res=false;

	/* retrieve addresses stored in seek record */
	SeekRecord* seekRecord=&(records[tid].ui);
	Node* ancestor=getPtr(seekRecord->ancestor);
	Node* successor=getPtr(seekRecord->successor);
	Node* parent=getPtr(seekRecord->parent);
	Node* leaf=getPtr(seekRecord->leaf);

	biptr<Node>* successorAddr=nullptr;
	biptr<Node>* childAddr=nullptr;
	biptr<Node>* siblingAddr=nullptr;

	/* obtain address of field of ancestor node that will be modified */
	if(nodeLess(&keyNode,ancestor))
		successorAddr=&(ancestor->left);
	else
		successorAddr=&(ancestor->right);

	/* obtain addresses of child fields of parent node */
	if(nodeLess(&keyNode,parent)){
		childAddr=&(parent->left);
		siblingAddr=&(parent->right);
	}
	else{
		childAddr=&(parent->right);
		siblingAddr=&(parent->left);
	}
	Node* tmpChild=childAddr->ptr();
	if(!getFlg(tmpChild)){
		/* the leaf is not flagged, thus sibling node should be flagged */
		tmpChild=siblingAddr->ptr();
		/* switch the sibling address */
		siblingAddr=childAddr;
	}

	/* use TAS to tag sibling edge */
	while(true){
		Node* untagged=siblingAddr->ptr();
		Node* tagged=mixPtrFlgTg(getPtr(untagged),getFlg(untagged),true);
		if(siblingAddr->CAS(untagged,tagged,std::memory_order_acq_rel)){
			break;
		}
	}
	/* read the flag and address fields */
	Node* tmpSibling=siblingAddr->ptr();

	/* make the sibling node a direct child of the ancestor node */
	res=successorAddr->CAS(successor,
		mixPtrFlgTg(getPtr(tmpSibling),getFlg(tmpSibling),false),
		std::memory_order_acq_rel);

	if(res==true){
		range_tracker->retire(getPtr(tmpChild),tid);
		range_tracker->retire(successor,tid);
	}
	return res;
}

/* to test rangeQuery */
// template <>
// optional<int> NatarajanTreeRangeTracker<int,int>::get(int key, int tid){
// 	int len=0;
// 	auto x = rangeQuery(key-500,key,len,tid);
// 	Node keyNode{key,defltV,nullptr,nullptr};//node to be compared
// 	optional<int> res={};
// 	SeekRecord* seekRecord=&(records[tid].ui);
// 	Node* leaf=nullptr;
// 	seek(key,tid);
// 	leaf=getPtr(seekRecord->leaf);
// 	if(nodeEqual(&keyNode,leaf)){
// 		res = leaf->val;
// 	}
// 	return res;
// }

template <class K, class V>
optional<V> NatarajanTreeRangeTracker<K,V>::get(K key, int tid){
	Node keyNode{key,defltV,nullptr,nullptr};//node to be compared
	optional<V> res={};
	SeekRecord* seekRecord=&(records[tid].ui);
	Node* leaf=nullptr;
	collect_retired_size(range_tracker->get_retired_cnt(tid), tid);
	range_tracker->reserve(tid);
	seek(key,tid);
	leaf=getPtr(seekRecord->leaf);
	if(nodeEqual(&keyNode,leaf)){
		res = leaf->val;
	}
	// range_tracker->clear_all(tid);
	range_tracker->clear(tid);
	return res;
}

template <class K, class V>
optional<V> NatarajanTreeRangeTracker<K,V>::put(K key, V val, int tid){
	optional<V> res={};
	SeekRecord* seekRecord=&(records[tid].ui);

	Node* newInternal=nullptr;
	Node* newLeaf=Node::alloc(key,val,nullptr,nullptr,range_tracker,tid);//also to compare keys
	Node* parent=nullptr;
	Node* leaf=nullptr;
	biptr<Node>* childAddr=nullptr;
	collect_retired_size(range_tracker->get_retired_cnt(tid), tid);
	range_tracker->reserve(tid);
	while(true){
		seek(key,tid);
		leaf=getPtr(seekRecord->leaf);
		parent=getPtr(seekRecord->parent);
		if(!nodeEqual(newLeaf,leaf)){//key does not exist
			/* obtain address of the child field to be modified */
			if(nodeLess(newLeaf,parent))
				childAddr=&(parent->left);
			else
				childAddr=&(parent->right);

			/* create left and right leave of newInternal */
			Node* newLeft=nullptr;
			Node* newRight=nullptr;
			if(nodeLess(newLeaf,leaf)){
				newLeft=newLeaf;
				newRight=leaf;
			}
			else{
				newLeft=leaf;
				newRight=newLeaf;
			}

			/* create newInternal */
			if(isInf(leaf)){
				int lev=getInfLevel(leaf);
				newInternal=Node::alloc(infK,defltV,newLeft,newRight,lev,range_tracker,tid);
			}
			else
				newInternal=Node::alloc(std::max(key,leaf->key),defltV,newLeft,newRight,range_tracker,tid);

			/* try to add the new nodes to the tree */
			Node* tmpExpected=getPtr(leaf);
			if(childAddr->CAS(tmpExpected,getPtr(newInternal),std::memory_order_acq_rel)){
				res={};
				break;//insertion succeeds
			}
			else{//fails; help conflicting delete operation
				range_tracker->reclaim(newInternal);
				Node* tmpChild=childAddr->ptr();
				if(getPtr(tmpChild)==leaf && (getFlg(tmpChild)||getTg(tmpChild))){
					/* 
					 * address of the child has not changed
					 * and either the leaf node or its sibling 
					 * has been flagged for deletion
					 */
					cleanup(key,tid);
				}
			}
		}
		else{//key exists, update and return old
			res=leaf->val;
			if(nodeLess(newLeaf,parent))
				childAddr=&(parent->left);
			else
				childAddr=&(parent->right);
			if(childAddr->CAS(leaf,newLeaf,std::memory_order_acq_rel)){
				range_tracker->retire(leaf,tid);
				break;
			}
		}
	}
	// range_tracker->clear_all(tid);
	range_tracker->clear(tid);
	return res;
}

template <class K, class V>
bool NatarajanTreeRangeTracker<K,V>::insert(K key, V val, int tid){
	bool res=false;
	SeekRecord* seekRecord=&(records[tid].ui);
	
	Node* newInternal=nullptr;
	Node* newLeaf=Node::alloc(key,val,nullptr,nullptr,range_tracker,tid);//also for comparing keys
	
	Node* parent=nullptr;
	Node* leaf=nullptr;
	biptr<Node>* childAddr=nullptr;
	collect_retired_size(range_tracker->get_retired_cnt(tid), tid);
	range_tracker->reserve(tid);
	while(true){
		seek(key,tid);
		leaf=getPtr(seekRecord->leaf);
		parent=getPtr(seekRecord->parent);
		if(!nodeEqual(newLeaf,leaf)){//key does not exist
			/* obtain address of the child field to be modified */
			if(nodeLess(newLeaf,parent))
				childAddr=&(parent->left);
			else
				childAddr=&(parent->right);

			/* create left and right leave of newInternal */
			Node* newLeft=nullptr;
			Node* newRight=nullptr;
			if(nodeLess(newLeaf,leaf)){
				newLeft=newLeaf;
				newRight=leaf;
			}
			else{
				newLeft=leaf;
				newRight=newLeaf;
			}

			/* create newInternal */
			if(isInf(leaf)){
				int lev=getInfLevel(leaf);
				newInternal=Node::alloc(infK,defltV,newLeft,newRight,lev,range_tracker,tid);
			}
			else
				newInternal=Node::alloc(std::max(key,leaf->key),defltV,newLeft,newRight,range_tracker,tid);

			/* try to add the new nodes to the tree */
			Node* tmpExpected=getPtr(leaf);
			if(childAddr->CAS(tmpExpected,getPtr(newInternal),std::memory_order_acq_rel)){
				res=true;
				break;
			}
			else{//fails; help conflicting delete operation
				range_tracker->reclaim(newInternal);
				Node* tmpChild=childAddr->ptr();
				if(getPtr(tmpChild)==leaf && (getFlg(tmpChild)||getTg(tmpChild))){
					/* 
					 * address of the child has not changed
					 * and either the leaf node or its sibling 
					 * has been flagged for deletion
					 */
					cleanup(key,tid);
				}
			}
		}
		else{//key exists, insertion fails
			range_tracker->reclaim(newLeaf);
			res=false;
			break;
		}
	}
	// range_tracker->clear_all(tid);
	range_tracker->clear(tid);
	return res;
}

template <class K, class V>
optional<V> NatarajanTreeRangeTracker<K,V>::remove(K key, int tid){
	bool injecting = true;
	optional<V> res={};
	SeekRecord* seekRecord=&(records[tid].ui);

	Node keyNode{key,defltV,nullptr,nullptr};//node to be compared
	
	Node* parent=nullptr;
	Node* leaf=nullptr;
	biptr<Node>* childAddr=nullptr;
	collect_retired_size(range_tracker->get_retired_cnt(tid), tid);
	range_tracker->reserve(tid);
	while(true){
		seek(key,tid);
		parent=getPtr(seekRecord->parent);
		/* obtain address of the child field to be modified */
		if(nodeLess(&keyNode,parent))
			childAddr=&(parent->left);
		else
			childAddr=&(parent->right);

		if(injecting){
			/* injection mode: check if the key exists */
			leaf=getPtr(seekRecord->leaf);
			if(!nodeEqual(leaf,&keyNode)){//does not exist
				res={};
				break;
			}

			/* inject the delete operation into the tree */
			Node* tmpExpected=getPtr(leaf);
			res=leaf->val;
			if(childAddr->CAS(tmpExpected,
				mixPtrFlgTg(tmpExpected,true,false), std::memory_order_acq_rel)){
				/* advance to cleanup mode to remove the leaf node */
				injecting=false;
				if(cleanup(key,tid)) break;
			}
			else{
				Node* tmpChild=childAddr->ptr();
				if(getPtr(tmpChild)==leaf && (getFlg(tmpChild)||getTg(tmpChild))){
					/*
					 * address of the child has not 
					 * changed and either the leaf
					 * node or its sibling has been
					 * flagged for deletion
					 */
					cleanup(key,tid);
				}
			}
		}
		else{
			/* cleanup mode: check if flagged node still exists */
			if(seekRecord->leaf!=leaf){
				/* leaf no longer in the tree */
				break;
			}
			else{
				/* leaf still in the tree; remove */
				if(cleanup(key,tid)) break;
			}
		}
	}
	// range_tracker->clear_all(tid);
	range_tracker->clear(tid);
	return res;
}

template <class K, class V>
optional<V> NatarajanTreeRangeTracker<K,V>::replace(K key, V val, int tid){
	optional<V> res={};
	SeekRecord* seekRecord=&(records[tid].ui);

	Node* newInternal=nullptr;
	Node* newLeaf=Node::alloc(key,val,nullptr,nullptr,range_tracker,tid);//also to compare keys

	Node* parent=nullptr;
	Node* leaf=nullptr;
	biptr<Node>* childAddr=nullptr;
	collect_retired_size(range_tracker->get_retired_cnt(tid), tid);
	range_tracker->reserve(tid);
	while(true){
		seek(key,tid);
		parent=getPtr(seekRecord->parent);
		leaf=getPtr(seekRecord->leaf);
		if(!nodeEqual(newLeaf,leaf)){//key does not exist, replace fails
			range_tracker->reclaim(newLeaf);
			res={};
			break;
		}
		else{//key exists, update and return old
			res=leaf->val;
			if(nodeLess(newLeaf,parent))
				childAddr=&(parent->left);
			else
				childAddr=&(parent->right);
			if(childAddr->CAS(leaf,newLeaf,std::memory_order_acq_rel)){
				range_tracker->retire(leaf,tid);
				break;
			}
		}
	}
	// range_tracker->clear_all(tid);
	range_tracker->clear(tid);
	return res;
}

template <class K, class V>
std::map<K, V> NatarajanTreeRangeTracker<K,V>::rangeQuery(K key1, K key2, int& len, int tid){
	//NOT HP-like GC safe.
	if(key1>key2) return {};
	Node k1{key1,defltV,nullptr,nullptr};//node to be compared
	Node k2{key2,defltV,nullptr,nullptr};//node to be compared

	collect_retired_size(range_tracker->get_retired_cnt(tid), tid);
	range_tracker->reserve(tid);
	
	Node* leaf=getPtr(range_tracker->read(s->left));
	Node* current=getPtr(range_tracker->read(leaf->left));

	std::map<K,V> res;
	if(current!=nullptr)
		doRangeQuery(k1,k2,tid,current,res);
	len=res.size();
	range_tracker->clear(tid);
	return res;
}

template <class K, class V>
void NatarajanTreeRangeTracker<K,V>::doRangeQuery(Node& k1, Node& k2, int tid, Node* root, std::map<K,V>& res){
	Node* left=getPtr(range_tracker->read(root->left));
	Node* right=getPtr(range_tracker->read(root->right));
	if(left==nullptr&&right==nullptr){
		if(nodeLessEqual(&k1,root)&&nodeLessEqual(root,&k2)){
			
			res.emplace(root->key,root->val);
		}
		return;
	}
	if(left!=nullptr){
		if(nodeLess(&k1,root)){
			doRangeQuery(k1,k2,tid,left,res);
		}
	}
	if(right!=nullptr){
		if(nodeLessEqual(root,&k2)){
			doRangeQuery(k1,k2,tid,right,res);
		}
	}
	return;
}
#endif