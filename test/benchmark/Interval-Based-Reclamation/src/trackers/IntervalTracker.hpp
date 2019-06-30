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



#ifndef INTERVAL_TRACKER_HPP
#define INTERVAL_TRACKER_HPP

#include <queue>
#include <list>
#include <vector>
#include <atomic>
#include "ConcurrentPrimitives.hpp"
#include "RAllocator.hpp"
#include "AllocatorMacro.hpp"

#include "BaseTracker.hpp"



template<class T> class IntervalTracker: public BaseTracker<T>{
private:
	int task_num;
	int freq;
	int epochFreq;
	bool collect;
	
public:
	class IntervalInfo{
	public:
		T* obj;
		uint64_t birth_epoch;
		uint64_t retire_epoch;
		IntervalInfo(T* obj, uint64_t b_epoch, uint64_t r_epoch):
		
			obj(obj), birth_epoch(b_epoch), retire_epoch(r_epoch){}
	};
	
private:
	paddedAtomic<uint64_t>* reservations;
	padded<uint64_t>* retire_counters;
	padded<uint64_t>* alloc_counters;
	padded<std::list<IntervalInfo>>* retired; 

	std::atomic<uint64_t> epoch;

public:
	~IntervalTracker(){};
	IntervalTracker(int task_num, int epochFreq, int emptyFreq, bool collect): 
	 BaseTracker<T>(task_num),task_num(task_num),freq(emptyFreq),epochFreq(epochFreq),collect(collect){
		retired = new padded<std::list<IntervalTracker<T>::IntervalInfo>>[task_num];
		reservations = new paddedAtomic<uint64_t>[task_num];
		retire_counters = new padded<uint64_t>[task_num];
		alloc_counters = new padded<uint64_t>[task_num];
		for (int i = 0; i<task_num; i++){
			reservations[i].ui.store(UINT64_MAX,std::memory_order_release);
			retired[i].ui.clear();
		}
		epoch.store(0,std::memory_order_release);
	}
	IntervalTracker(int task_num, int epochFreq, int emptyFreq) : IntervalTracker(task_num,epochFreq,emptyFreq,true){}
	

	void __attribute__ ((deprecated)) reserve(uint64_t e, int tid){
		return start_op(tid);
	}
	uint64_t getEpoch(){
		return epoch.load(std::memory_order_acquire);
	}
	void* alloc(int tid){
		alloc_counters[tid]=alloc_counters[tid]+1;
		if(alloc_counters[tid]%(epochFreq*task_num)==0){
			epoch.fetch_add(1,std::memory_order_acq_rel);
		}
		//return (void*)malloc(sizeof(T));
		char* block = (char*) PM_malloc(sizeof(uint64_t) + sizeof(T));
		uint64_t* birth_epoch = (uint64_t*)(block + sizeof(T));
		*birth_epoch = getEpoch();
		return (void*)block;
	}
	uint64_t read_birth(T* obj){
		uint64_t* birth_epoch = (uint64_t*)((char*)obj + sizeof(T));
		return *birth_epoch;
	}
	void reclaim(T* obj){
		obj->~T();
		PM_free ((char*)obj);
	}
	void start_op(int tid){
		uint64_t e = epoch.load(std::memory_order_acquire);
		reservations[tid].ui.store(e,std::memory_order_seq_cst);
	}
	void end_op(int tid){
		reservations[tid].ui.store(UINT64_MAX,std::memory_order_seq_cst);
		
	}
	void reserve(int tid){
		start_op(tid);
	}
	void clear(int tid){
		end_op(tid);
	}
	bool validate(int tid){
		return (reservations[tid].ui.load(std::memory_order_acquire) == 
			epoch.load(std::memory_order_acquire));
	}


	inline void incrementEpoch(){
		epoch.fetch_add(1,std::memory_order_acq_rel);
	}
	
	
	void retire(T* obj, uint64_t birth_epoch, int tid){
		if(obj==NULL){return;}
		std::list<IntervalInfo>* myTrash = &(retired[tid].ui);
		// for(auto it = myTrash->begin(); it!=myTrash->end(); it++){
		// 	assert(it->obj!=obj && "double retire error");
		// }
			
		uint64_t retire_epoch = epoch.load(std::memory_order_acquire);
		IntervalInfo info = IntervalInfo(obj, birth_epoch, retire_epoch);
		myTrash->push_back(info);	
		if(collect && retire_counters[tid]%freq==0){
			empty(tid);
		}
		retire_counters[tid]=retire_counters[tid]+1;
	}

	void retire(T* obj, int tid){
		retire(obj, read_birth(obj), tid);
	}

	bool conflict(uint64_t* reservEpoch, uint64_t birth_epoch, uint64_t retire_epoch){
		for (int i = 0; i < task_num; i++){
			if (reservEpoch[i] >= birth_epoch && reservEpoch[i] <= retire_epoch){
				return true;
			}
		}
		return false;
	}
	
	void empty(int tid){
		//read all epochs
		uint64_t reservEpoch[task_num];
		for (int i = 0; i < task_num; i++){
			reservEpoch[i] = reservations[i].ui.load(std::memory_order_acquire);
		}
		
		// erase safe objects
		std::list<IntervalInfo>* myTrash = &(retired[tid].ui);
		for (auto iterator = myTrash->begin(), end = myTrash->end(); iterator != end; ) {
			IntervalInfo res = *iterator;
			if(!conflict(reservEpoch, res.birth_epoch, res.retire_epoch)){
				iterator = myTrash->erase(iterator);
				this->reclaim(res.obj);
				this->dec_retired(tid);
			}
			else{++iterator;}
		}
	}
		
	bool collecting(){return collect;}
	
};


#endif
