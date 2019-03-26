#ifndef _LOCKFREE_STACK_HPP_
#define _LOCKFREE_STACK_HPP_

#include <atomic>
#include "optional.hpp"
#include "pm_config.hpp"
#include "HazardPointers.hpp"


/*
 * Lock-free LIFO Stack
 * 
 * The paper on Hazard Pointers is named "Hazard Pointers: Safe Memory
 * Reclamation for Lock-Free objects" and it is available here:
 * http://web.cecs.pdx.edu/~walpole/class/cs510/papers/11.pdf
 */

template<typename T>
class LockfreeStack {

private:
	struct Node {
		optional<T> item;
		std::atomic<Node*> next;

		Node(optional<T> userItem = {}) : item{userItem}, next{nullptr} { }

		bool casNext(Node *cmp, Node *val) {
			return next.compare_exchange_strong(cmp, val);
		}

	};

	bool casTop(Node *cmp, Node *val) {
		return top.compare_exchange_strong(cmp, val);
	}

	// Pointers to top of the stack
	alignas(CACHE_LINE_SIZE) std::atomic<Node*> top;

	const int maxThreads;

	// We need one hazard pointers for pop()
	HazardPointers<Node> hp {1, maxThreads};
	const int kHpTop = 0;

public:
	LockfreeStack(int maxThreads=MAX_THREADS) : maxThreads{maxThreads} {
		top.store(nullptr, std::memory_order_relaxed);
	}

	~LockfreeStack() {
		while (pop(0)); // Drain the stack
	}

	std::string className() { return "LockfreeStack"; }

	void push(T item, const int tid) {
		Node* newNode = new Node(item);
		while (true) {
			Node* ltop = top.load();
			newNode->next.store(ltop);
			if(casTop(ltop,newNode)) return;
		}
	}

	optional<T> pop(const int tid) {
		while(true){
			Node* ltop = hp.protectPtr(kHpTop, top, tid);
			if(ltop != nullptr){
				if(ltop != top.load()) continue;//to ensure we protect correct top
				Node* lnext = ltop->next.load();
				if(casTop(ltop,lnext)) {
					T item = ltop->item.value(); // Another thread may clean up lnext after we do hp.clear()
					hp.clear(tid);
					hp.retire(ltop, tid);
					return item;
				}
			} else{
				hp.clear(tid);
				return {};
			}

		}
	}
};

#endif