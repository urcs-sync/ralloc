#ifndef _ARRAYSTACK_HPP_
#define _ARRAYSTACK_HPP_

#include "RegionManager.hpp"

#include <atomic>
#include <cassert>
#include "optional.hpp"
#include <iostream>

#include "concurrentprimitives.hpp"
/*
 *********class ArrayStack<T>*********
 * This is a nonblocking array-based stack using unbounded counter.
 *
 * I use RegionManager to map the entire stack in a contiguous region
 * in order to share and reuse it in persistent memory.
 *
 * Requirement: 
 * 	1.	It uses 128-bit atomic primitives such as 128CAS.
 * 		Please make sure your machine and compiler support it 
 * 		and the flag -latomic is set when you compile.
 * 	2.	The capacity of the stack is fixed and is decided by 
 * 		const variable FREELIST_CAP defined in pm_config.hpp.
 * 		assert failure will be triggered when overflow.
 *
 * template parameter T:
 * 		T must be a type == 64 bit
 * 		T must be some type convertible to int, like pointers
 * Functions:
 * 		ArrayStack(string id): 
 * 			Create a stack and map it to file:
 * 			${HEAPFILE_PREFIX}_stack_${id}
 * 			e.g. /dev/shm/_stack_freesb
 * 		~ArrayStack(): 
 * 			Destroy ArrayStack object but not the real stack.
 * 			Data in real stack will be flushed properly.
 * 		void push(T val): 
 * 			Push val to the stack.
 * 		optional<T> pop(): 
 * 			Pop the top value from the stack, or empty.
 * 		cleanup(): 
 * 			Do the work as destructor. 
 *
 * It's from paper: 
 * 		Non-blocking Array-based Algorithms for Stacks and Queues
 * by 
 * 		Niloufar Shafiei
 *
 * Implemented by:
 * 		Wentao Cai (wcai6@cs.rochester.edu)
 */
template <class T, int size> class _ArrayStack;
template <class T, int size=FREESTACK_CAP>
class ArrayStack {
public:
	ArrayStack(std::string _id):id(_id){
		if(sizeof(T)>8) assert(0&&"type T larger than one word!");
		std::string path = HEAPFILE_PREFIX + std::string("_stack_")+id;
#ifdef GC//we don't need online flush
		bool persist = false;
#else//we need eager flush
		bool persist = true;
#endif
		if(RegionManager::exists_test(path)){
			mgr = new RegionManager(path,2*sizeof(_ArrayStack<T,size>),persist);
			void* hstart = mgr->__fetch_heap_start();
			_stack = (_ArrayStack<T,size>*) hstart;
#ifdef GC//we call GC to bring dirty queue back to life
			if(_stack->clean == false){
				//todo: call gc to reconstruct the stack
				assert(0&&"stack is dirty and recovery isn't implemented yet");
			}
#endif
		} else {
			//doesn't exist. create a new one
			mgr = new RegionManager(path,2*sizeof(_ArrayStack<T,size>),persist);
			bool res = mgr->__nvm_region_allocator((void**)&_stack,PAGESIZE,sizeof(_ArrayStack<T,size>));
			if(!res) assert(0&&"mgr allocation fails!");
			mgr->__store_heap_start(_stack);
			new (_stack) _ArrayStack<T,size>();
		}
	};
	~ArrayStack(){
		cleanup();
	};
	void cleanup(){
		FLUSHFENCE;
		_stack->clean = true;
		FLUSH(&_stack->clean);
		FLUSHFENCE;
		delete mgr;
	}
	void push(T val){_stack->push(val);};
	optional<T> pop(){return _stack->pop();};
private:
	const std::string id;
	/* manager to map, remap, and unmap the heap */
	RegionManager* mgr;//initialized when ArrayStack constructs
	_ArrayStack<T,size>* _stack;
};

/*
 * This is the helper function for ArrayStack which 
 * really does the job of an array stack.
 * template parameter:
 * 		T: type of elements contained
 * 		size: capacity of the stack. default is FREELIST_CAP
 */
template <class T, int size>
class _ArrayStack {
public:
	bool clean = false;
	_ArrayStack():top(),nodes(){};
	~_ArrayStack(){};
	void push(T val){
		while(true){
			Top old_top = top.load(std::memory_order_acquire);
			TFLUSH(&top);
			finish(old_top.value, old_top.index, old_top.counter);
			if(old_top.index == size - 1) 
				assert(0&&"stack is full!");
			Top new_top(val,old_top.index+1,nodes[old_top.index+1].load(std::memory_order_acquire).counter+1);
			TFLUSH(&nodes[old_top.index+1]);
			TFLUSHFENCE;
			if(top.compare_exchange_weak(old_top,new_top,std::memory_order_acq_rel)){
				TFLUSH(&top);
				TFLUSHFENCE;
				return;
			}
		}
	}
	optional<T> pop(){
		while(true){
			Top old_top = top.load(std::memory_order_acquire);
			TFLUSH(&top);
			finish(old_top.value, old_top.index, old_top.counter);
			if(old_top.index == 0) 
				return {};
			Node below_top = nodes[old_top.index-1].load(std::memory_order_acquire);
			Top new_top(below_top.value,old_top.index-1,below_top.counter+1);
			TFLUSH(&nodes[old_top.index-1]);
			TFLUSHFENCE;
			if(top.compare_exchange_weak(old_top,new_top,std::memory_order_acq_rel)) {
				TFLUSH(&top);
				TFLUSHFENCE;
				return old_top.value;
			}
		}
	}
	void cleanup(){
		FLUSHFENCE;
		clean = true;
		FLUSH(&clean);
		FLUSHFENCE;
	}
	void init(){
		top.store({0,0,0});
	}
private:
	void finish(T value, uint32_t index, uint32_t counter){
		Node old_node(nodes[index].load(std::memory_order_acquire).value, counter-1);
		Node new_node(value, counter);
		TFLUSH(&nodes[index]);
		TFLUSHFENCE;
		nodes[index].compare_exchange_strong(old_node,new_node,std::memory_order_acq_rel);
		TFLUSH(&nodes[index]);
		TFLUSHFENCE;
		return;
	}
	struct Top{
		T value;
		uint32_t index;
		uint32_t counter;
		Top(T val=(T)0, uint32_t a=0, uint32_t b=0) noexcept:
			value(val),
			index(a),
			counter(b){};
	};
	struct Node{
		T value;
		uint64_t counter;//This must be 64bit, or CAS will spuriously fail.
		Node(T val = (T)0, uint32_t a = 0) noexcept:
			value(val),
			counter(a){};
	};
	std::atomic<Top> top;
	std::atomic<Node> nodes[size];
};

#endif