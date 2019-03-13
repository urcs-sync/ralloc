#ifndef _ARRAYSTACK_HPP_
#define _ARRAYSTACK_HPP_

#include "RegionManager.hpp"
#include <cassert>
#include "optional.hpp"
#include <iostream>
/*
 *********class ArrayStack<T>*********
 * This is a nonblocking array-based stack.
 *
 * I use RegionManager to map the entire stack in a contiguous region
 * in order to share and reuse it in persistent memory.
 *
 * Requirement: 
 * 	1.	It uses 128-bit CAS.
 * 		Please make sure your machine support cmpxchg16b
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
 *
 * It's from paper: 
 * 		Non-blocking Array-based Algorithms for Stacks and Queues
 * by 
 * 		Niloufar Shafiei
 *
 * Implemented by:
 * 		Wentao Cai (wcai6@cs.rochester.edu)
 */
template <class T, int size=FREELIST_CAP> class _ArrayStack;
template <class T>
class ArrayStack {
public:
	ArrayStack(std::string _id):id(_id){
		if(sizeof(T)>8) assert(0&&"type T larger than one word!");
		string path = HEAPFILE_PREFIX + string("_stack_")+id;
		if(RegionManager::exists_test(path)){
			mgr = new RegionManager(path,false);
			void* hstart = mgr->__fetch_heap_start();
			_stack = (_ArrayStack<T>*) hstart;
			if(_stack->clean == false){
				//todo: call gc to reconstruct the stack
				assert(0&&"stack is dirty and recovery isn't implemented yet");
			}
		} else {
			//doesn't exist. create a new one
			mgr = new RegionManager(path,false);
			bool res = mgr->__nvm_region_allocator((void**)&_stack,PAGESIZE,sizeof(_ArrayStack<T>));
			if(!res) assert(0&&"mgr allocation fails!");
			mgr->__store_heap_start(_stack);
			new (_stack) _ArrayStack<T>();
		}
	};
	~ArrayStack(){
		_stack->clean = true;
		FLUSH(&_stack->clean);
		FLUSHFENCE;
		delete mgr;
	};
	void push(T val){_stack->push(val);};
	optional<T> pop(){return _stack->pop();};
private:
	const std::string id;
	/* manager to map, remap, and unmap the heap */
	RegionManager* mgr;//initialized when ArrayStack constructs
	_ArrayStack<T>* _stack;
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
			Top old_top = top.load();
			finish(old_top.value, old_top.index_counter.index, old_top.index_counter.counter);
			if(old_top.index_counter.index == size - 1) 
				assert(0&&"stack is full!");
			Top new_top(val,old_top.index_counter.index+1,nodes[old_top.index_counter.index+1].load().counter.counter+1);
fuck:
			if(top.WideCAS(old_top,new_top))
				return;
		}
	}
	optional<T> pop(){
		while(true){
			Top old_top = top.load();
			finish(old_top.value, old_top.index_counter.index, old_top.index_counter.counter);
			if(old_top.index_counter.index == 0) 
				return {};
			Node below_top = nodes[old_top.index_counter.index-1].load();
			Top new_top(below_top.value,old_top.index_counter.index-1,below_top.counter.counter+1);
			if(top.WideCAS(old_top,new_top)) 
				return old_top.value;
		}
	}
private:
	void finish(T value, uint32_t index, uint32_t counter){
		Node old_node(nodes[index].load().value, counter-1);
		Node new_node(value, counter);
		nodes[index].WideCAS(old_node,new_node);
		return;
	}
	struct Top{
		struct IndexCounter{
			uint32_t index;
			uint32_t counter;
			IndexCounter(uint32_t a=0, uint32_t b=0) noexcept:
				index(a),counter(b){};
		};
		T value;
		IndexCounter index_counter;
		Top(T val=(T)0, uint32_t a=0, uint32_t b=0) noexcept:
			value(val),
			index_counter(a,b){};
#if (__x86_64__ || __ppc64__)
		inline bool WideCAS(Top &old_value, 
			Top &new_value, std::memory_order morder) {
			bool ret;
			__asm__ __volatile__(
			"lock cmpxchg16b %1;\n"
			"sete %0;\n"
			:"=m"(ret),"+m" (*(volatile Top *) (this))
			:"a" (old_value.value), "d" (old_value.index_counter), "b" (new_value.value), "c" (new_value.index_counter));
			std::atomic_thread_fence(morder);
			return ret;
		}
#else
		inline bool WideCAS(Top &old_value, 
			Top &new_value, std::memory_order morder) {
			errexit("WCAS not supported with -m32.");
		}
#endif
		inline bool WideCAS(Top &old_value, Top &new_value){
			return WideCAS(old_value, new_value, std::memory_order_seq_cst);
		}
		inline Top load(std::memory_order morder){
			Top ret = *this;
			std::atomic_thread_fence(morder);
			return ret;
		}
		inline Top load(){
			return load(std::memory_order_seq_cst);
		}
	}__attribute__((aligned(CACHE_LINE_SIZE)));
	struct Node{
		struct Counter{
			uint64_t counter;
			Counter(uint32_t a) noexcept:
				counter(a){};
		};
		T value;
		Counter counter;
		Node(T val = (T)0, uint32_t a = 0) noexcept:
			value(val),
			counter(a){};
#if (__x86_64__ || __ppc64__)
		inline bool WideCAS(Node &old_value, 
			Node &new_value, std::memory_order morder) {
			bool ret;
			__asm__ __volatile__(
			"lock cmpxchg16b %1;\n"
			"sete %0;\n"
			:"=m"(ret),"+m" (*(volatile Node *) (this))
			:"a" (old_value.value), "d" (old_value.counter), "b" (new_value.value), "c" (new_value.counter));
			std::atomic_thread_fence(morder);
			return ret;
		}
#else
		inline bool WideCAS(Node &old_value, 
			Node &new_value, std::memory_order morder) {
			errexit("WCAS not supported with -m32.");
		}
#endif
		inline bool WideCAS(Node &old_value, Node &new_value){
			return WideCAS(old_value, new_value, std::memory_order_seq_cst);
		}
		inline Node load(std::memory_order morder){
			Node ret = *this;
			std::atomic_thread_fence(morder);
			return ret;
		}
		inline Node load(){
			return load(std::memory_order_seq_cst);
		}
	}__attribute__((aligned(CACHE_LINE_SIZE)));
	Top top;
	Node nodes[size];
};

#endif