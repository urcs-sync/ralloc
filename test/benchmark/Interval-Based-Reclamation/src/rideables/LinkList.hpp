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
 * This is a pure linked list that only supports put and remove
 * Differing from other RUnorderedMap, put pushes a new node to the head, and
 * remove pop the head node.
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
	}__attribute__((aligned(CACHELINE_SIZE)));
	atomic_pptr<Node> head;
	// to protect retired node and avoid ABA problem happening on head node
	MemoryTracker<Node>* memory_tracker;
public:
	LinkedList(GlobalTestConfig* gtc):
		RetiredMonitorable(gtc){
		int epochf = gtc->getEnv("epochf").empty()? 150:stoi(gtc->getEnv("epochf"));
		int emptyf = gtc->getEnv("emptyf").empty()? 30:stoi(gtc->getEnv("emptyf"));
		memory_tracker = new MemoryTracker<Node>(gtc, epochf, emptyf, 1, COLLECT);
		}
	~LinkedList(){};
	Node* mkNode(K k, V v, Node* n, int tid){
		void* ptr = memory_tracker->alloc(tid);
		return new (ptr) Node(k, v, n);
	}

	optional<V> get(K key, int tid){return {};}
	optional<V> put(K key, V val, int tid){
		Node* new_head = mkNode(key,val,nullptr,tid);
		memory_tracker->start_op(tid);
		Node* old_head = memory_tracker->read(head, 0, tid);
		do {
			new_head->next.store(old_head);
		} while(!head.compare_exchange_weak(old_head, new_head));
		memory_tracker->end_op(tid);
		memory_tracker->clear_all(tid);
		return {};
	}
	bool insert(K key, V val, int tid){
		Node* new_head = mkNode(key,val,nullptr,tid);
		memory_tracker->start_op(tid);
		Node* old_head = memory_tracker->read(head, 0, tid);
		do {
			new_head->next.store(old_head);
		} while(!head.compare_exchange_weak(old_head, new_head));
		memory_tracker->end_op(tid);
		memory_tracker->clear_all(tid);
		return true;
	}
	// This is a trivial remove: we pop the head node no matter what key is the input
	optional<V> remove(K key, int tid){
		memory_tracker->start_op(tid);
		while(true) {
			Node* old_head = memory_tracker->read(head, 0, tid);
			Node* new_head = old_head->next.load(); // we don't need to protect this node since 
			if(head.compare_exchange_weak(old_head, new_head)){
				memory_tracker->retire(old_head, tid);
				break;
			}
		}
		memory_tracker->end_op(tid);
		memory_tracker->clear_all(tid);
		return {};
	}
	optional<V> replace(K key, V val, int tid){return {};}
	void restart(GlobalTestConfig* gtc){
		retired_cnt = new padded<uint64_t>[gtc->task_num];
		int epochf = gtc->getEnv("epochf").empty()? 150:stoi(gtc->getEnv("epochf"));
		int emptyf = gtc->getEnv("emptyf").empty()? 30:stoi(gtc->getEnv("emptyf"));
		memory_tracker = new MemoryTracker<Node>(gtc, epochf, emptyf, 1, COLLECT);
	}
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
			ret->restart(gtc);
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
