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


#ifndef SGL_UNORDEREDMAP
#define SGL_UNORDEREDMAP


#include <unordered_map>
#include <atomic>
#include <string>
#include "Harness.hpp"
#include "RUnorderedMap.hpp"

template <class K, class V> class SGLUnorderedMap : public RUnorderedMap<K,V>{
private:
	// Simple test and set lock
	// There are better ways to do this...
	inline void lockAcquire(int tid){
		int unlk = -1;
		while(!lk.compare_exchange_strong(unlk, tid,std::memory_order::memory_order_acq_rel)){
			unlk = -1; // compare_exchange puts the old value into unlk, so set it back
		}
		assert(lk.load()==tid);
	}

	inline void lockRelease(int tid){
		assert(lk==tid);
		int unlk = -1;
		lk.store(unlk,std::memory_order::memory_order_release);
	}

	std::unordered_map<K,V>* m=NULL;
	std::atomic<int> lk;


public:

	SGLUnorderedMap(){
		m = new std::unordered_map<K,V>();
		lk.store(-1,std::memory_order::memory_order_release);
	}
	~SGLUnorderedMap(){}

	bool insert(K key, V val, int tid){
		lockAcquire(tid);
		auto v = m->emplace(key,val);
		lockRelease(tid);
		return v.second;
	}

	optional<V> put(K key, V val, int tid){
		optional<V> res = {};
		lockAcquire(tid);
		auto it = m->find(key);
		if(it != m->end()){
			res = it->second;
		}
		m->operator[](key)=val;
		lockRelease(tid);
		return res;
	}

	optional<V> replace(K key, V val, int tid){
		optional<V> res = {};
		lockAcquire(tid);
		auto v = m->find(key);
		if(v != m->end()){
			res = v->second;
			m->operator[](key)=val;
		}
		lockRelease(tid);
		return res;
	}

	optional<V> remove(K key,int tid){
		optional<V> res = {};
		lockAcquire(tid);
		auto v = m->find(key);
		if(v != m->end()){
			res = v->second;
			m->erase(key);
		}
		lockRelease(tid);
		return res;
	}

	optional<V> get(K key, int tid){
		optional<V> res = {};
		lockAcquire(tid);
		auto v = m->find(key);
		if(v != m->end()){
			res = v->second;
		}
		lockRelease(tid);
		return res;
	}

};

template <class K, class V> class SGLUnorderedMapFactory : public RideableFactory{
	SGLUnorderedMap<K,V>* build(GlobalTestConfig* gtc){
		return new SGLUnorderedMap<K,V>();
	}
};

#endif
