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


#ifndef LINK_LIST
#define LINK_LIST


#include <atomic>
#include "Harness.hpp"
#include "ConcurrentPrimitives.hpp"
#include "RUnorderedMap.hpp"
#include "HazardTracker.hpp"
#include "MemoryTracker.hpp"
#include "RetiredMonitorable.hpp"
#include "pptr.hpp"
#include "trackers/AllocatorMacro.hpp"
#include <functional>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#ifdef NGC
#define COLLECT false
#else
#define COLLECT true
#endif


/*
 * This is a pure linked list that only supports put.
 * Differing from other RUnorderedMap, put pushes a new node to the head.
 * This is only for testing GC time in different data size.
 */

template <class K, class V>
class LinkedList : public RUnorderedMap<K,V>, public RetiredMonitorable {
	friend class GarbageCollection;
	struct Node{
		K key;
		V val;
		atomic_pptr<Node> next;
		Node(){};
		Node(K k, V v, Node* n):key(k),val(v),next(n){};
		void* operator new(size_t size){
			return PM_malloc(size);
		}
		void operator delete(void* ptr){
			PM_free(ptr);
		}
	};
	// we don't consider ABA problem since we only put Node but never delete
	atomic_pptr<Node> head;
public:
	LinkedList(GlobalTestConfig* gtc):
		RetiredMonitorable(gtc){};
	~LinkedList(){};


	optional<V> get(K key, int tid){return {};}
	optional<V> put(K key, V val, int tid){
		Node* old_head = head.load();
		Node* new_head = new Node(key,val,old_head);
		do {
			new_head->next.store(old_head);
		} while(!head.compare_exchange_weak(old_head, new_head));
		return {};
	}
	bool insert(K key, V val, int tid){
		Node* old_head = head.load();
		Node* new_head = new Node(key,val,old_head);
		do {
			new_head->next.store(old_head);
		} while(!head.compare_exchange_weak(old_head, new_head));
		return true;
	}
	optional<V> remove(K key, int tid){return {};}
	optional<V> replace(K key, V val, int tid){return {};}
	void* operator new(size_t size){
		return PM_malloc(size);
	}
	void operator delete(void* ptr){
		PM_free(ptr);
	}
};

template <class K, class V>
class LinkListFactory : public RideableFactory{
	LinkedList<K,V>* build(GlobalTestConfig* gtc){
		if(gtc->restart && get_root(2) != nullptr) {
			auto ret = reinterpret_cast<LinkedList<K,V>*>(get_root(2));
			return ret;
		} else {
			auto ret = new LinkedList<K,V>(gtc);
			set_root(ret, 2);
			return ret;
		}
	}
};

template<>
inline void GarbageCollection::filter_func(LinkedList<int,int>* ptr) {
	return mark_func(ptr->head.load());
}

template<>
inline void GarbageCollection::filter_func(LinkedList<int,int>::Node* ptr) {
	return mark_func(ptr->next.load());
}

#endif
