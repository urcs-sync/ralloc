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


#ifndef SORTED_UNORDEREDMAP_RANGE
#define SORTED_UNORDEREDMAP_RANGE

#include <atomic>
#include "Harness.hpp"
#include "ConcurrentPrimitives.hpp"
#include "RUnorderedMap.hpp"

#include "RangeTracker.hpp"
// #include "RangeTracker.hpp"

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
class SortedUnorderedMapRange : public RUnorderedMap<K,V>, public RetiredMonitorable{
	struct Node;

	struct MarkPtr{
		biptr<Node> ptr;
		MarkPtr(Node* n):ptr(n){};
		MarkPtr():ptr(nullptr){};
	};

	struct Node{
		K key;
		V val;
		MarkPtr next;
		Node(K k, V v, Node* n):key(k),val(v),next(n){};
	};
private:
	std::hash<K> hash_fn;
	const int idxSize;
	padded<MarkPtr>* bucket=new padded<MarkPtr>[idxSize]{};
	bool findNode(MarkPtr* &prev, Node* &cur, Node* &nxt, K key, int tid);

	RangeTracker<Node>* range_tracker;

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

	inline Node* mkNode(K key, V val, Node* next, int tid){
		void* obj = range_tracker->alloc(tid);
		return new (obj) Node(key, val, next);
	}

public:
	SortedUnorderedMapRange(GlobalTestConfig* gtc, int idx_size): 
	RetiredMonitorable(gtc), idxSize(idx_size){
		int epochf = gtc->getEnv("epochf").empty()? 150:stoi(gtc->getEnv("epochf"));
		int emptyf = gtc->getEnv("emptyf").empty()? 30:stoi(gtc->getEnv("emptyf"));
		range_tracker = new RangeTracker<Node>(gtc, epochf, emptyf, true);
	};
	~SortedUnorderedMapRange(){};

	optional<V> get(K key, int tid);
	optional<V> put(K key, V val, int tid);
	bool insert(K key, V val, int tid);
	optional<V> remove(K key, int tid);
	optional<V> replace(K key, V val, int tid);
};

template <class K, class V> 
class SortedUnorderedMapRangeFactory : public RideableFactory{
	SortedUnorderedMapRange<K,V>* build(GlobalTestConfig* gtc){
		return new SortedUnorderedMapRange<K,V>(gtc,30000);
	}
};


//-------Definition----------
template <class K, class V> 
optional<V> SortedUnorderedMapRange<K,V>::get(K key, int tid) {
	MarkPtr* prev=nullptr;
	Node* cur=nullptr;
	Node* nxt=nullptr;
	optional<V> res={};

	collect_retired_size(range_tracker->get_retired_cnt(tid), tid);
	range_tracker->reserve(tid);

	if(findNode(prev,cur,nxt,key,tid)){
		res=cur->val;
	}
	return res;
} 

template <class K, class V> 
optional<V> SortedUnorderedMapRange<K,V>::put(K key, V val, int tid) {
	Node* tmpNode = nullptr;
	MarkPtr* prev=nullptr;
	Node* cur=nullptr;
	Node* nxt=nullptr;
	optional<V> res={};
	tmpNode = mkNode(key, val, nullptr, tid);

	collect_retired_size(range_tracker->get_retired_cnt(tid), tid);
	range_tracker->reserve(tid);

	while(true){
		if(findNode(prev,cur,nxt,key,tid)){
			/* found the node; replace */
			res=cur->val;
			tmpNode->next.ptr = cur;
			/* insert tmpNode */
			if(prev->ptr.CAS(cur,tmpNode,std::memory_order_acq_rel)){
				/* mark cur */
				while(!cur->next.ptr.CAS(nxt,setMk(nxt),std::memory_order_acq_rel));//mark cur
				/* detach cur */
				if(tmpNode->next.ptr.CAS(cur,nxt,std::memory_order_acq_rel)){
					range_tracker->retire(cur, tid);
				}
				else{
					findNode(prev,cur,nxt,key,tid);
				}
				break;
			}
		}
		else{
			/* not found; insert */
			res={};
			tmpNode->next.ptr = cur;
			/* insert tmpNode */
			if(prev->ptr.CAS(cur,tmpNode,std::memory_order_acq_rel)){
				break;
			}
		}
	}

	range_tracker->clear(tid);

	return res;
}

template <class K, class V> 
bool SortedUnorderedMapRange<K,V>::insert(K key, V val, int tid){
	Node* tmpNode = nullptr;
	MarkPtr* prev=nullptr;
	Node* cur=nullptr;
	Node* nxt=nullptr;
	bool res=false;
	tmpNode = mkNode(key, val, nullptr, tid);

	collect_retired_size(range_tracker->get_retired_cnt(tid), tid);
	range_tracker->reserve(tid);

	while(true){
		if(findNode(prev,cur,nxt,key,tid)){
			/* found the node; break */
			res=false;
			range_tracker->reclaim(tmpNode);
			break;
		}
		else{
			/* not found; insert */
			tmpNode->next.ptr = cur;
			/* insert tmpNode */
			if(prev->ptr.CAS(cur,tmpNode,std::memory_order_acq_rel)){
				res=true;
				break;
			}
		}
	}

	range_tracker->clear(tid);

	return res;
}

template <class K, class V> 
optional<V> SortedUnorderedMapRange<K,V>::remove(K key, int tid) {
	MarkPtr* prev=nullptr;
	Node* cur=nullptr;
	Node* nxt=nullptr;
	optional<V> res={};

	collect_retired_size(range_tracker->get_retired_cnt(tid), tid);
	range_tracker->reserve(tid);

	while(true){
		if(!findNode(prev,cur,nxt,key,tid)){
			/* not found; break */
			res={};
			break;
		}
		res=cur->val;
		/* mark cur */
		if(!cur->next.ptr.CAS(nxt,setMk(nxt),std::memory_order_acq_rel))
			continue;
		/* detach cur */
		if(prev->ptr.CAS(cur,nxt,std::memory_order_acq_rel)){
			range_tracker->retire(cur, tid);
		}
		else{
			findNode(prev,cur,nxt,key,tid);
		}
		break;
	}

	range_tracker->clear(tid);

	// myHazard->clearAll(tid);
	return res;
}

template <class K, class V> 
optional<V> SortedUnorderedMapRange<K,V>::replace(K key, V val, int tid) {
	Node* tmpNode = nullptr;
	MarkPtr* prev=nullptr;
	Node* cur=nullptr;
	Node* nxt=nullptr;
	optional<V> res={};
	tmpNode = mkNode(key, val, nullptr, tid);

	collect_retired_size(range_tracker->get_retired_cnt(tid), tid);
	range_tracker->reserve(tid);


	while(true){
		if(findNode(prev,cur,nxt,key,tid)){
			/* found the node; replace */
			res=cur->val;
			tmpNode->next.ptr = cur;
			/* insert tmpNode */
			if(prev->ptr.CAS(cur,tmpNode,std::memory_order_acq_rel)){
				/* mark cur */
				while(!cur->next.ptr.CAS(nxt,setMk(nxt),std::memory_order_acq_rel));//mark cur
				/* detach cur */
				if(tmpNode->next.ptr.CAS(cur,nxt,std::memory_order_acq_rel)){
					range_tracker->retire(cur, tid);
				}
				else{
					findNode(prev,cur,nxt,key,tid);
				}
				break;
			}
		}
		else{
			/* not found; break */
			res={};
			range_tracker->reclaim(tmpNode);
			break;
		}
	}

	range_tracker->clear(tid);
	
	return res;
}

template <class K, class V> 
bool SortedUnorderedMapRange<K,V>::findNode(MarkPtr* &prev, Node* &cur, Node* &nxt, K key, int tid){
	while(true){
		size_t idx=hash_fn(key)%idxSize;
		bool cmark=false;
		prev=&bucket[idx].ui;
		/* to read prev to cur and reserve */
		do{
			cur=getPtr(prev->ptr.ptr());
		}while(prev->ptr.ptr()!=cur);

		while(true){
			/* 
			 * to reserve old and cur
			 *
			 * for details, please refer to the paper Michael[2002]
			 */
			if(cur==nullptr) return false;
			// nxt=cur->next.ptr.load(std::memory_order_acquire);
			nxt=cur->next.ptr.ptr();
			cmark=getMk(nxt);
			nxt=getPtr(nxt);
			if(mixPtrMk(nxt,cmark)!=cur->next.ptr.ptr())
				break;//return findNode(prev,cur,nxt,key,tid);
			auto ckey=cur->key;
			if(prev->ptr.ptr()!=cur)
				break;//return findNode(prev,cur,nxt,key,tid);
			if(!cmark){
				if(ckey>=key) return ckey==key;
				prev=&(cur->next);
			}
			else{
				if(prev->ptr.CAS(cur,nxt,std::memory_order_acq_rel))
					range_tracker->retire(cur, tid);
				else
					break;//return findNode(prev,cur,nxt,key,tid);
			}
			cur=nxt;
		}
	}
}
#endif
