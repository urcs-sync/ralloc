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



#ifndef HAZARD_TRACKER_HPP
#define HAZARD_TRACKER_HPP

#ifndef _REENTRANT
#define _REENTRANT
#endif

#include <queue>
#include <list>
#include <vector>
#include <atomic>
#include "ConcurrentPrimitives.hpp"
#include "RAllocator.hpp"
#include "AllocatorMacro.hpp"

#include "BaseTracker.hpp"


template<class T>
class HazardTracker: public BaseTracker<T>{
private:
	int task_num;
	int slotsPerThread;
	int freq;
	bool collect;

	RAllocator* mem;

	paddedAtomic<T*>* slots;
	padded<int>* cntrs;
	padded<std::list<T*>>* retired; // TODO: use different structure to prevent malloc locking....

	void empty(int tid){
		std::list<T*>* myTrash = &(retired[tid].ui);
		for (typename std::list<T*>::iterator iterator = myTrash->begin(), end = myTrash->end(); iterator != end; ) {
			bool danger = false;
			auto ptr = *iterator;
			for (int i = 0; i<task_num*slotsPerThread; i++){
				if(ptr == slots[i].ui){
					danger = true;
					break;
				}
			}
			if(!danger){
				this->reclaim(ptr);
				this->dec_retired(tid);
				iterator = myTrash->erase(iterator);
			}
			else{++iterator;}
		}
		return;
	}

public:
	~HazardTracker(){};
	HazardTracker(int task_num, int slotsPerThread, int emptyFreq, bool collect):BaseTracker<T>(task_num){
		this->task_num = task_num;
		this->slotsPerThread = slotsPerThread;
		this->freq = emptyFreq;
		slots = new paddedAtomic<T*>[task_num*slotsPerThread];
		for (int i = 0; i<task_num*slotsPerThread; i++){
			slots[i]=NULL;
		}
		retired = new padded<std::list<T*>>[task_num];
		cntrs = new padded<int>[task_num];
		for (int i = 0; i<task_num; i++){
			cntrs[i]=0;
			retired[i].ui = std::list<T*>();
		}
		this->collect = collect;
	}
	HazardTracker(int task_num, int slotsPerThread, int emptyFreq): 
		HazardTracker(task_num, slotsPerThread, emptyFreq, true){}

	T* read(std::atomic<T*>& obj, int idx, int tid){
		T* ret;
		T* realptr;
		while(true){
			ret = obj.load(std::memory_order_acquire);
			realptr = (T*)((size_t)ret & 0xfffffffffffffffc);
			reserve(realptr, idx, tid);
			if(ret == obj.load(std::memory_order_acquire)){
				return ret;
			}
		}
	}

	T* read(const atomic_pptr<T>& obj, int idx, int tid){
		T* ret;
		T* realptr;
		while(true){
			ret = obj.load(std::memory_order_acquire);
			realptr = (T*)((size_t)ret & 0xfffffffffffffffc);
			reserve(realptr, idx, tid);
			if(ret == obj.load(std::memory_order_acquire)){
				return ret;
			}
		}
	}

	void reserve(T* ptr, int slot, int tid){
		slots[tid*slotsPerThread+slot] = ptr;
	}
	void clearSlot(int slot, int tid){
		slots[tid*slotsPerThread+slot] = NULL;
	}
	void clearAll(int tid){
		for(int i = 0; i<slotsPerThread; i++){
			slots[tid*slotsPerThread+i] = NULL;
		}
	}
	void clear_all(int tid){
		clearAll(tid);
	}

	void retire(T* ptr, int tid){
		if(ptr==NULL){return;}
		std::list<T*>* myTrash = &(retired[tid].ui);
		// for(auto it = myTrash->begin(); it!=myTrash->end(); it++){
		// 	assert(*it !=ptr && "double retire error");
		// }
		myTrash->push_back(ptr);	
		if(collect && cntrs[tid]==freq){
			cntrs[tid]=0;
			empty(tid);
		}
		cntrs[tid].ui++;
	}
	

	bool collecting(){return collect;}
	
};


#endif
