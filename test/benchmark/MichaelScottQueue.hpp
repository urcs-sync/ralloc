/******************************************************************************
 * Copyright (c) 2014-2016, Pedro Ramalhete, Andreia Correia
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *	 * Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *	 * Redistributions in binary form must reproduce the above copyright
 *	   notice, this list of conditions and the following disclaimer in the
 *	   documentation and/or other materials provided with the distribution.
 *	 * Neither the name of Concurrency Freaks nor the
 *	   names of its contributors may be used to endorse or promote products
 *	   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************
 */

#ifndef _MICHAEL_SCOTT_QUEUE_HPP_
#define _MICHAEL_SCOTT_QUEUE_HPP_

#include <atomic>
#include <stdexcept>
#include "optional.hpp"
#include "pm_config.hpp"
#include "HazardPointers.hpp"


/**
 * <h1> Michael-Scott Queue </h1>
 *
 * enqueue algorithm: MS enqueue
 * dequeue algorithm: MS dequeue
 * Consistency: Linearizable
 * enqueue() progress: lock-free
 * dequeue() progress: lock-free
 * Memory Reclamation: Hazard Pointers (lock-free)
 *
 *
 * Maged Michael and Michael Scott's Queue with Hazard Pointers
 * <p>
 * Lock-Free Linked List as described in Maged Michael and Michael Scott's paper:
 * {@link http://www.cs.rochester.edu/~scott/papers/1996_PODC_queues.pdf}
 * <a href="http://www.cs.rochester.edu/~scott/papers/1996_PODC_queues.pdf">
 * Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms</a>
 * <p>
 * The paper on Hazard Pointers is named "Hazard Pointers: Safe Memory
 * Reclamation for Lock-Free objects" and it is available here:
 * http://web.cecs.pdx.edu/~walpole/class/cs510/papers/11.pdf
 *
 */
template<typename T>
class MichaelScottQueue {

private:
	struct Node {
		optional<T> item;
		std::atomic<Node*> next;

		Node(optional<T> userItem = {}) : item{userItem}, next{nullptr} { }

		bool casNext(Node *cmp, Node *val) {
			return next.compare_exchange_strong(cmp, val);
		}
	};

	bool casTail(Node *cmp, Node *val) {
		return tail.compare_exchange_strong(cmp, val);
	}

	bool casHead(Node *cmp, Node *val) {
		return head.compare_exchange_strong(cmp, val);
	}

	// Pointers to head and tail of the list
	alignas(CACHELINE_SIZE) std::atomic<Node*> head;
	alignas(CACHELINE_SIZE) std::atomic<Node*> tail;

	const int maxThreads;

	// We need two hazard pointers for dequeue()
	HazardPointers<Node> hp {2, maxThreads};
	const int kHpTail = 0;
	const int kHpHead = 0;
	const int kHpNext = 1;

public:
	MichaelScottQueue(int maxThreads) : maxThreads{maxThreads} {
		Node* sentinelNode = new Node();
		head.store(sentinelNode, std::memory_order_relaxed);
		tail.store(sentinelNode, std::memory_order_relaxed);
	}


	~MichaelScottQueue() {
		while (dequeue(0)); // Drain the queue
		delete head.load(); // Delete the last node
	}

	std::string className() { return "MichaelScottQueue"; }

	void enqueue(T item, const int tid) {
		Node* newNode = new Node(item);
		while (true) {
			Node* ltail = hp.protectPtr(kHpTail, tail, tid);
			if (ltail == tail.load()) {//to ensure we protect correct tail
				Node* lnext = ltail->next.load();
				if (lnext == nullptr) {
					// It seems this is the last node, so add the newNode here
					// and try to move the tail to the newNode
					if (ltail->casNext(nullptr, newNode)) {
						casTail(ltail, newNode);
						hp.clear(tid);
						return;
					}
				} else {
					casTail(ltail, lnext);
				}
			}
		}
	}

	optional<T> dequeue(const int tid) {
		while(true){
			Node* lhead = hp.protect(kHpHead, head, tid);
			if(lhead != head.load()) continue;//to ensure we protect correct head
			Node* ltail = tail.load();
			Node* lnext = hp.protect(kHpNext, lhead->next, tid);
			if(lhead != head.load()) continue;//to ensure next is correct
			if(lnext == nullptr){
				hp.clear(tid);
				return {}; // Queue is empty
			}
			if(lhead != ltail){
				if(casHead(lhead,lnext)){
					T item = lnext->item.value(); // Another thread may clean up lnext after we do hp.clear()
					hp.clear(tid);
					hp.retire(lhead, tid);
					return item;
				}
			} else {
				casTail(ltail, lnext);
			}
		}
	}
};

#endif /* _MICHAEL_SCOTT_QUEUE_HPP_ */
