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



#ifndef RANGE_TRACKER_HPP
#define RANGE_TRACKER_HPP

#include <queue>
#include <list>
#include <vector>
#include <atomic>
#include "ConcurrentPrimitives.hpp"
#include "RAllocator.hpp"
#include "BlockPool.hpp"

enum UpdateType{LF, FAA, WCAS, TP};

template<class T> class RangeTracker;

#include "biptr.hpp"

template<class T> class RangeTracker{
private:
	int task_num;
	int freq;
	int epochFreq;
	bool collect;
	static __thread int local_tid;
public:
	UpdateType type;
	class IntervalInfo{
	public:
		T* obj;
		uint64_t birth_epoch;
		uint64_t retire_epoch;
		IntervalInfo(T* obj, uint64_t b_epoch, uint64_t r_epoch):
			obj(obj), birth_epoch(b_epoch), retire_epoch(r_epoch){}
	};
	
private:
	paddedAtomic<uint64_t>* upper_reservs;
	paddedAtomic<uint64_t>* lower_reservs;
	padded<uint64_t>* retire_counters;
	padded<uint64_t>* alloc_counters;
	padded<std::list<IntervalInfo>>* retired;
	padded<uint64_t>* retired_cnt;

	std::atomic<uint64_t> epoch;

public:
	~RangeTracker(){};
	RangeTracker(GlobalTestConfig* gtc, int epochFreq, int emptyFreq, bool collect): 
	 task_num(gtc->task_num),freq(emptyFreq),epochFreq(epochFreq),collect(collect){
		retired = new padded<std::list<RangeTracker<T>::IntervalInfo>>[task_num];
		retired_cnt = new padded<uint64_t>[task_num];

		upper_reservs = new paddedAtomic<uint64_t>[task_num];
		lower_reservs = new paddedAtomic<uint64_t>[task_num];
		for (int i = 0; i < task_num; i++){
			upper_reservs[i].ui.store(UINT64_MAX, std::memory_order_release);
			lower_reservs[i].ui.store(UINT64_MAX, std::memory_order_release);
		}
		retire_counters = new padded<uint64_t>[task_num];
		alloc_counters = new padded<uint64_t>[task_num];
		// regist tracker into biptr, so biptr can call update before dereferencing.
		biptr<T>::set_tracker(this);

		// get and deal with types.
		type = get_update_type(gtc);
		epoch.store(0,std::memory_order_release);
	}
	RangeTracker(int task_num, int epochFreq, int emptyFreq) : RangeTracker(task_num,epochFreq,emptyFreq,true){}

	UpdateType get_update_type(GlobalTestConfig* gtc){
		if (gtc->getEnv("tracker").empty()){
			return LF;
		} else if (gtc->getEnv("tracker") == "LF"){
			return LF;
		} else if (gtc->getEnv("tracker") == "FAA"){
			return FAA;
		} else if (gtc->getEnv("tracker") == "WCAS"){
			return WCAS;
		} else {
			errexit("rangetracker constructor - tracker type error.");
			exit(1);
		}
	}

	uint64_t get_retired_cnt(int tid){
		return retired_cnt[tid].ui;
	}
	
	static void setLocalTid(int tid){
		local_tid = tid;
	}
	static int getLocalTid(){
		return local_tid;
	}

	void __attribute__ ((deprecated)) reserve(uint64_t e, int tid){
		return reserve(tid);
	}
	uint64_t get_epoch(){
		return epoch.load(std::memory_order_acquire);
	}

	void* alloc(int tid){
		alloc_counters[tid] = alloc_counters[tid]+1;
		if(alloc_counters[tid]%(epochFreq*task_num)==0){
			epoch.fetch_add(1,std::memory_order_acq_rel);
		}
		switch(type){
			default:
				char* block = (char*) malloc(sizeof(uint64_t) + sizeof(T));
				uint64_t* birth_epoch = (uint64_t*)(block + sizeof(T));
				*birth_epoch = get_epoch();
				return (void*)block;
			break;
		}
	}

	static uint64_t read_birth(T* obj){
		uint64_t* birth_epoch = (uint64_t*)((char*)obj + sizeof(T));
		return *birth_epoch;
	}

	void reclaim(T* obj){
		if (!obj) return;
		obj->~T();
		free ((char*)obj);
	}

	void reserve(int tid){
		uint64_t e = epoch.load(std::memory_order_acquire);
		// lower_reservs[tid].ui.store(e,std::memory_order_release);
		// upper_reservs[tid].ui.store(e,std::memory_order_release);
		upper_reservs[tid].ui.store(e,std::memory_order_seq_cst);
		lower_reservs[tid].ui.store(e,std::memory_order_seq_cst);
		setLocalTid(tid);
	}
	void update_reserve(uint64_t e){ //called by biptr.
		uint64_t curr_upper = upper_reservs[local_tid].ui.load(std::memory_order_acquire);
		if (e > curr_upper){
			// upper_reservs[local_tid].ui.store(e, std::memory_order_release);
			upper_reservs[local_tid].ui.store(e, std::memory_order_seq_cst);
		} else {
			return;
		}
	}

	bool validate(uint64_t birth_before){//TODO: add validation before every dereference.
		return (upper_reservs[local_tid].ui.load(std::memory_order_acquire) >= birth_before);
	}
	void clear(int tid){
		upper_reservs[tid].ui.store(UINT64_MAX,std::memory_order_seq_cst);
		lower_reservs[tid].ui.store(UINT64_MAX,std::memory_order_seq_cst);
	}

	inline void incrementEpoch(){
		epoch.fetch_add(1,std::memory_order_acq_rel);
	}

	T* read(biptr<T> ptr){
		return ptr.protect_and_fetch_ptr();
	}
	
	void retire(T* obj, uint64_t birth_epoch, int tid){
		if(obj==NULL){return;}
		std::list<IntervalInfo>* myTrash = &(retired[tid].ui);
		// for(auto it = myTrash->begin(); it!=myTrash->end(); it++){
		// 	assert(it->obj!=obj && "double retire error");
		// }
			
		uint64_t retire_epoch = epoch.load(std::memory_order_acquire);
		myTrash->push_back(IntervalInfo(obj, birth_epoch, retire_epoch));
		if(collect && retire_counters[tid]%freq==0){
			empty(tid);
		}
		retire_counters[tid]=retire_counters[tid]+1;
		retired_cnt[tid].ui++;
	}

	void retire(T* obj, int tid){
		retire(obj, read_birth(obj), tid);
	}
	
	bool conflict(uint64_t* lower_epochs, uint64_t* upper_epochs, uint64_t birth_epoch, uint64_t retire_epoch){
		for (int i = 0; i < task_num; i++){
			if (upper_epochs[i] >= birth_epoch && lower_epochs[i] <= retire_epoch){
				return true;
			}
		}
		return false;
	}

	void empty(int tid){
		uint64_t upper_epochs_arr[task_num];
		uint64_t lower_epochs_arr[task_num];
		for (int i = 0; i < task_num; i++){
			lower_epochs_arr[i] = lower_reservs[i].ui.load(std::memory_order_acquire);
			upper_epochs_arr[i] = upper_reservs[i].ui.load(std::memory_order_acquire);
		}

		// erase safe objects
		std::list<IntervalInfo>* myTrash = &(retired[tid].ui);
		for (auto iterator = myTrash->begin(), end = myTrash->end(); iterator != end; ) {
			IntervalInfo res = *iterator;
			if(!conflict(lower_epochs_arr, upper_epochs_arr, res.birth_epoch, res.retire_epoch)){
				reclaim(res.obj);
				retired_cnt[tid].ui--;
				iterator = myTrash->erase(iterator);
			}
			else{++iterator;}
		}
	}

	bool collecting(){return collect;}
};


//requirement for linkers.
template<class T>
__thread int RangeTracker<T>::local_tid;

template<class T>
RangeTracker<T>* biptr<T>::range_tracker;

#endif
