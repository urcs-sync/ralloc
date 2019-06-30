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


#ifndef SORTED_UNORDEREDMAP
#define SORTED_UNORDEREDMAP

#include <atomic>
#include "Harness.hpp"
#include "ConcurrentPrimitives.hpp"
#include "RUnorderedMap.hpp"
#include "HazardTracker.hpp"
#include "MemoryTracker.hpp"
#include "RetiredMonitorable.hpp"
#include <functional>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#ifdef NGC
#define COLLECT false
#else
#define COLLECT true
#endif

template <class K, class V>
class SortedUnorderedMap : public RUnorderedMap<K,V>, public RetiredMonitorable{
	struct Node;

	struct MarkPtr{
		std::atomic<Node*> ptr;
		MarkPtr(Node* n):ptr(n){};
		MarkPtr():ptr(nullptr){};
	};

	struct Node{
		K key;
		V val;
		MarkPtr next;
		Node(){};
		Node(K k, V v, Node* n):key(k),val(v),next(n){};
	};
private:
	std::hash<K> hash_fn;
	const int idxSize;
	padded<MarkPtr>* bucket=new padded<MarkPtr>[idxSize]{};
	bool findNode(MarkPtr* &prev, Node* &cur, Node* &nxt, K key, int tid);
	
	MemoryTracker<Node>* memory_tracker;

	const size_t GET_POINTER_BITS = 0xfffffffffffffffe;
	inline Node* getPtr(Node* mptr){
		return (Node*) ((size_t)mptr & GET_POINTER_BITS);
	}
	inline bool getMk(Node* mptr){
		return (bool)((size_t)mptr & 1);
	}
	inline Node* mixPtrMk(Node* ptr, bool mk){
		return (Node*) ((size_t)ptr | mk);
	}
	inline Node* setMk(Node* mptr){
		return mixPtrMk(mptr,true);
	}

public:
	SortedUnorderedMap(GlobalTestConfig* gtc,int idx_size):
		RetiredMonitorable(gtc),idxSize(idx_size){
        int epochf = gtc->getEnv("epochf").empty()? 150:stoi(gtc->getEnv("epochf"));
        int emptyf = gtc->getEnv("emptyf").empty()? 30:stoi(gtc->getEnv("emptyf"));
		std::cout<<"emptyf:"<<emptyf<<std::endl;
		memory_tracker = new MemoryTracker<Node>(gtc, epochf, emptyf, 3, COLLECT);
	}
	~SortedUnorderedMap(){};

	Node* mkNode(K k, V v, Node* n, int tid){
		void* ptr = memory_tracker->alloc(tid);
		return new (ptr) Node(k, v, n);
	}


	optional<V> get(K key, int tid);
	optional<V> put(K key, V val, int tid);
	bool insert(K key, V val, int tid);
	optional<V> remove(K key, int tid);
	optional<V> replace(K key, V val, int tid);
};

template <class K, class V> 
class SortedUnorderedMapFactory : public RideableFactory{
	SortedUnorderedMap<K,V>* build(GlobalTestConfig* gtc){
		return new SortedUnorderedMap<K,V>(gtc,30000);
	}
};

//-------Definition----------
template <class K, class V> 
optional<V> SortedUnorderedMap<K,V>::get(K key, int tid) {
	MarkPtr* prev=nullptr;
	Node* cur=nullptr;
	Node* nxt=nullptr;
	optional<V> res={};

	collect_retired_size(memory_tracker->get_retired_cnt(tid), tid);

	memory_tracker->start_op(tid);
	if(findNode(prev,cur,nxt,key,tid)){
		res=cur->val;
	}
	memory_tracker->clear_all(tid);
	memory_tracker->end_op(tid);
	return res;
}

template <class K, class V> 
optional<V> SortedUnorderedMap<K,V>::put(K key, V val, int tid) {
	Node* tmpNode = nullptr;
	MarkPtr* prev=nullptr;
	Node* cur=nullptr;
	Node* nxt=nullptr;
	optional<V> res={};
	tmpNode = mkNode(key, val, nullptr, tid);

	collect_retired_size(memory_tracker->get_retired_cnt(tid), tid);

	memory_tracker->start_op(tid);
	while(true){
		if(findNode(prev,cur,nxt,key,tid)){
			res=cur->val;
			tmpNode->next.ptr.store(cur,std::memory_order_release);
			if(prev->ptr.compare_exchange_strong(cur,tmpNode,std::memory_order_acq_rel)){
				while(!cur->next.ptr.compare_exchange_strong(nxt,setMk(nxt),std::memory_order_acq_rel));//mark cur
				if(tmpNode->next.ptr.compare_exchange_strong(cur,nxt,std::memory_order_acq_rel)){
					memory_tracker->retire(cur, tid);
				}
				else{
					findNode(prev,cur,nxt,key,tid);
				}
				break;
			}
		}
		else{//does not exist, insert.
			res={};
			tmpNode->next.ptr.store(cur,std::memory_order_release);
			if(prev->ptr.compare_exchange_strong(cur,tmpNode,std::memory_order_acq_rel)){
				break;
			}
		}
	}
	memory_tracker->end_op(tid);
	memory_tracker->clear_all(tid);
	return res;
}

template <class K, class V> 
bool SortedUnorderedMap<K,V>::insert(K key, V val, int tid){
	Node* tmpNode = nullptr;
	MarkPtr* prev=nullptr;
	Node* cur=nullptr;
	Node* nxt=nullptr;
	bool res=false;
	tmpNode = mkNode(key, val, nullptr, tid);

	collect_retired_size(memory_tracker->get_retired_cnt(tid), tid);

	memory_tracker->start_op(tid);
	while(true){
		if(findNode(prev,cur,nxt,key,tid)){
			res=false;
			memory_tracker->reclaim(tmpNode, tid);
			break;
		}
		else{//does not exist, insert.
			tmpNode->next.ptr.store(cur,std::memory_order_release);
			if(prev->ptr.compare_exchange_strong(cur,tmpNode,std::memory_order_acq_rel)){
				res=true;
				break;
			}
		}
	}
	memory_tracker->end_op(tid);
	memory_tracker->clear_all(tid);
	return res;
}

template <class K, class V> 
optional<V> SortedUnorderedMap<K,V>::remove(K key, int tid) {
	MarkPtr* prev=nullptr;
	Node* cur=nullptr;
	Node* nxt=nullptr;
	optional<V> res={};

	collect_retired_size(memory_tracker->get_retired_cnt(tid), tid);

	memory_tracker->start_op(tid);
	while(true){
		if(!findNode(prev,cur,nxt,key,tid)){
			res={};
			break;
		}
		res=cur->val;
		if(!cur->next.ptr.compare_exchange_strong(nxt,setMk(nxt),std::memory_order_acq_rel))
			continue;
		if(prev->ptr.compare_exchange_strong(cur,nxt,std::memory_order_acq_rel)){
			memory_tracker->retire(cur, tid);
		}
		else{
			findNode(prev,cur,nxt,key,tid);
		}
		break;
	}
	memory_tracker->end_op(tid);
	memory_tracker->clear_all(tid);
	return res;
}

template <class K, class V> 
optional<V> SortedUnorderedMap<K,V>::replace(K key, V val, int tid) {
	Node* tmpNode = nullptr;
	MarkPtr* prev=nullptr;
	Node* cur=nullptr;
	Node* nxt=nullptr;
	optional<V> res={};
	tmpNode = mkNode(key, val, nullptr, tid);

	collect_retired_size(memory_tracker->get_retired_cnt(tid), tid);

	memory_tracker->start_op(tid);
	while(true){
		if(findNode(prev,cur,nxt,key,tid)){
			res=cur->val;
			tmpNode->next.ptr.store(cur,std::memory_order_release);
			if(prev->ptr.compare_exchange_strong(cur,tmpNode,std::memory_order_acq_rel)){
				while(!cur->next.ptr.compare_exchange_strong(nxt,setMk(nxt),std::memory_order_acq_rel));//mark cur
				if(tmpNode->next.ptr.compare_exchange_strong(cur,nxt,std::memory_order_acq_rel)){
					// myHazard->(cur,tid);
					memory_tracker->retire(cur, tid);
				}
				else{
					findNode(prev,cur,nxt,key,tid);
				}
				break;
			}
		}
		else{//does not exist
			res={};
			memory_tracker->reclaim(tmpNode, tid);
			break;
		}
	}
	memory_tracker->end_op(tid);
	memory_tracker->clear_all(tid);
	return res;
}

template <class K, class V> 
bool SortedUnorderedMap<K,V>::findNode(MarkPtr* &prev, Node* &cur, Node* &nxt, K key, int tid){
	while(true){
		size_t idx=hash_fn(key)%idxSize;
		bool cmark=false;
		prev=&bucket[idx].ui;

		cur=getPtr(memory_tracker->read(prev->ptr, 1, tid));

		while(true){//to lock old and cur
			if(cur==nullptr) return false;
			nxt=memory_tracker->read(cur->next.ptr, 0, tid);
			cmark=getMk(nxt);
			nxt=getPtr(nxt);
			if(mixPtrMk(nxt,cmark)!=memory_tracker->read(cur->next.ptr, 1, tid))
				break;//return findNode(prev,cur,nxt,key,tid);
			auto ckey=cur->key;
			if(memory_tracker->read(prev->ptr, 2, tid)!=cur)
				break;//return findNode(prev,cur,nxt,key,tid);
			if(!cmark){
				if(ckey>=key) return ckey==key;
				prev=&(cur->next);
			}
			else{
				if(prev->ptr.compare_exchange_strong(cur,nxt,std::memory_order_acq_rel))
					memory_tracker->retire(cur, tid);
				else
					break;//return findNode(prev,cur,nxt,key,tid);
			}
			cur=nxt;
		}
	}
}
#endif
