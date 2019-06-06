#ifndef _ARRAYQUEUE_HPP_
#define _ARRAYQUEUE_HPP_

#include "RegionManager.hpp"

#include <cassert>
#include <atomic>
// optional should be provided by c++17 but for compatibility
// optional.hpp comes with the library in src
#include "optional.hpp"
#include <iostream>

#include "concurrentprimitives.hpp"
/*
 *********class ArrayQueue<T>*********
 * This is a nonblocking array-based queue using unbounded counter.
 *
 * I use RegionManager to map the entire queue in a contiguous region
 * in order to share and reuse it in persistent memory.
 *
 * Requirement: 
 * 	1.	It uses 128-bit atomic primitives such as 128CAS.
 * 		Please make sure your machine and compiler support it 
 * 		and the flag -latomic is set when you compile.
 * 	2.	The capacity of the queue is fixed and is decided by 
 * 		const variable FREELIST_CAP defined in pm_config.hpp.
 * 		assert failure will be triggered when overflow.
 *
 * template parameter T:
 * 		T must be a type == 64 bit
 * 		T must be some type convertible to int, like pointers
 * Functions:
 * 		ArrayQueue(string id): 
 * 			Create a queue and map it to file:
 * 			${HEAPFILE_PREFIX}_queue_${id}
 * 			e.g. /dev/shm/_queue_freesb
 * 		~ArrayQueue(): 
 * 			Destroy ArrayQueue object but not the real queue.
 * 			Data in real queue will be flushed properly.
 * 		void push(T val): 
 * 			Push val to the queue.
 * 		optional<T> pop(): 
 * 			Pop the top value from the queue, or empty.
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
template <class T, int size> class _ArrayQueue;
template <class T, int size=FREEQUEUE_CAP>
class ArrayQueue {
public:
	ArrayQueue(std::string _id):id(_id){
		if(sizeof(T)>8) assert(0&&"type T larger than one word!");
		std::string path = HEAPFILE_PREFIX + std::string("_queue_")+id;
#ifdef GC//we don't need online flush
		bool persist = false;
#else//we need eager flush
		bool persist = true;
#endif
		if(RegionManager::exists_test(path)){
			mgr = new RegionManager(path,sizeof(_ArrayQueue<T,size>),persist);
			void* hstart = mgr->__fetch_heap_start();
			_queue = (_ArrayQueue<T,size>*) hstart;
#ifdef GC//we call GC to bring dirty queue back to life
			if(_queue->clean == false){
				//todo: call gc to reconstruct the queue
				assert(0&&"queue is dirty and recovery isn't implemented yet");
			}
#endif
		} else {
			//doesn't exist. create a new one
			mgr = new RegionManager(path,sizeof(_ArrayQueue<T,size>),persist);
			bool res = mgr->__nvm_region_allocator((void**)&_queue,PAGESIZE,sizeof(_ArrayQueue<T,size>));
			if(!res) assert(0&&"mgr allocation fails!");
			mgr->__store_heap_start(_queue);
			new (_queue) _ArrayQueue<T,size>();
		}
	};
	~ArrayQueue(){
		cleanup();
	};
	void cleanup(){
		FLUSHFENCE;
		_queue->clean = true;
		FLUSH(&_queue->clean);
		FLUSHFENCE;
		delete mgr;
	}
	void push(T val){_queue->push(val);};
	optional<T> pop(){return _queue->pop();};
private:
	const std::string id;
	/* manager to map, remap, and unmap the heap */
	RegionManager* mgr;//initialized when ArrayQueue constructs
	_ArrayQueue<T,size>* _queue;
};

/*
 * This is the helper function for ArrayQueue which 
 * really does the job of an array queue.
 * template parameter:
 * 		T: type of elements contained
 * 		size: capacity of the queue. default is FREELIST_CAP
 */
template <class T, int size>
class _ArrayQueue {
public:
	bool clean = false;
	_ArrayQueue():rear(),front(),nodes(){};
	~_ArrayQueue(){};
	void push(T val){
		while(true){
			Rear old_rear;
			Front old_front;
			while(true){
				old_rear = rear.load(std::memory_order_acquire);
				old_front = front.load(std::memory_order_acquire);
				if(old_rear == rear.load(std::memory_order_acquire))
					break;
			}
			TFLUSH(&front);
			TFLUSH(&rear);

			finish_enqueue(old_rear.value, old_rear.index, old_rear.counter);
			if(old_front.index == (old_rear.index+2)%size)
				assert(0&&"queue is full!");
			Rear new_rear( val, (old_rear.index+1)%size, nodes[(old_rear.index+1)%size].load(std::memory_order_acquire).counter+1 );
			TFLUSHFENCE;

			if(rear.compare_exchange_weak(old_rear,new_rear,std::memory_order_acq_rel)){
				TFLUSH(&rear);
				TFLUSHFENCE;
				return;
			}
		}
	}
	optional<T> pop(){
		while(true){
			Front old_front;
			Rear old_rear;
			while(true){
				old_front = front.load(std::memory_order_acquire);
				old_rear = rear.load(std::memory_order_acquire);
				if(old_front == front.load(std::memory_order_acquire))
					break;
			}
			TFLUSH(&front);
			TFLUSH(&rear);

			if(old_front.index == old_rear.index)
				finish_enqueue(old_rear.value, old_rear.index, old_rear.counter);
			if(old_front.index == (old_rear.index+1)%size)
				return {};//empty
			T ret = nodes[old_front.index%size].load(std::memory_order_acquire).value;
			Front new_front((old_front.index+1)%size, old_front.counter+1);
			TFLUSH(&nodes[old_front.index%size]);
			TFLUSHFENCE;

			if(front.compare_exchange_strong(old_front,new_front,std::memory_order_acq_rel)){
				TFLUSH(&front);
				TFLUSHFENCE;
				return ret;
			}
		}
	}
private:
	void finish_enqueue(T value, uint32_t index, uint32_t counter){
		Node old_node(nodes[index].load(std::memory_order_acquire).value,counter-1);
		Node new_node(value,counter);

		TFLUSH(&nodes[index]);
		TFLUSHFENCE;
		nodes[index].compare_exchange_strong(old_node,new_node,std::memory_order_acq_rel);
		TFLUSH(&nodes[index]);
		TFLUSHFENCE;

		return;
	}
	struct Rear{
		T value;
		uint32_t index;
		uint32_t counter;
		Rear(T val=(T)0, uint32_t a=0, uint32_t b=0) noexcept:
			value(val),
			index(a),
			counter(b){};
		bool operator==(const Rear& rhs){
			return value == rhs.value && index == rhs.index && counter == rhs.counter;
		}
	};
	struct Front{
		uint32_t index;
		uint32_t counter;
		Front(uint32_t a=1, uint32_t b=0) noexcept:
			index(a),
			counter(b){};
		bool operator==(const Front& rhs){
			return index == rhs.index && counter == rhs.counter;
		}
	};
	struct Node{
		T value;
		uint64_t counter;//This must be 64bit, or CAS will spuriously fail.
		Node(T val = (T)0, uint32_t a = 0) noexcept:
			value(val),
			counter(a){};
	};
	paddedAtomic<Rear> rear;
	paddedAtomic<Front> front;
	std::atomic<Node> nodes[size];
};

#endif