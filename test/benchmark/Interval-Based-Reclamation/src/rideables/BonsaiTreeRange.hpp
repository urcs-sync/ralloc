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


#ifndef BONSAI_TREE_RANGE
#define BONSAI_TREE_RANGE


#include <atomic>
#include <string>
#include <list>
#include "Harness.hpp"
#include "ROrderedMap.hpp"

#include "RangeTracker.hpp"
// #include "RangeTracker.hpp"

#include "RetiredMonitorable.hpp"

#define WEIGHT 2 //TODO: make it a variable later.

template <class K, class V> class BonsaiTreeRange : public ROrderedMap<K, V>, public RetiredMonitorable{
private:

	class State;

	class Node{
	public:
		//might be a node in the tree, 
		biptr<Node> left;
		biptr<Node> right;
		K key;
		V value;
		unsigned long size;
		//or a wrapper to a State node.
		State* state = NULL;
		Node* next_state = NULL;
		
		Node();
		Node(Node* l, Node* r, K k, V v);
		Node(State* state);
		~Node();
	};



	class State{
	private:
		void doCleanState(Node* r);

	public:
		biptr<Node> root;
		Node* next;
		std::list<Node*> retire_list_prev;
		std::list<Node*> retire_list;
		std::list<Node*> new_list;

		RangeTracker<Node>* range_tracker;

		State(RangeTracker<Node>* range_tracker);
		State(Node* r, Node* n);
		~State();
		void addNewNode(Node* new_node);
	};

	enum Operation { op_insert, op_put, op_replace, op_remove };

	
	biptr<Node> curr_state;

	static __thread int local_tid;

	Node* protect_read(biptr<Node>& read);
	optional<V> update(Operation op, K key, V val, int tid);
	Node* doInsert(Node* state, Node* root, K key, V value, bool* ret);
	Node* doPut(Node* state, Node* root, K key, V value, V** ori_val);
	Node* doReplace(Node* state, Node* root, K key, V value, V** ori_val);
	Node* doRemove(Node* state, Node *node, K key, V** ori_val);

	Node* mkState();
	void killNewState(Node* state);
	
	Node* mkNode(State* state);
	Node* mkNode(Node* state, Node* left, Node* right, K key, V value);
	unsigned long nodeSize(Node* node);

	//routines for balancing
	Node* mkBalanced(Node* state, Node* node, Node* left, Node *right);
	Node* mkBalancedL(Node* state, Node* left, Node* right, K key, V value);
	Node* mkBalancedR(Node* state, Node* left, Node* right, K key, V value);
	Node* singleL(Node* state, Node* left, Node* right, Node* right_left, Node* right_right, K key, V value);
	Node* doubleL(Node* state, Node* left, Node* right, Node* right_left, Node* right_right, K key, V value);
	Node* singleR(Node* state, Node* left, Node* right, Node* left_right, Node* left_left, K key, V value);
	Node* doubleR(Node* state, Node* left, Node* right, Node* left_right, Node* left_left, K key, V value);

	//routines for removing intermediate nodes
	Node* pullLeftMost(Node* state, Node* node, Node** successor);
	Node* pullRightMost(Node* state, Node* node, Node** successor);

	//routine for rangeQuery
	void doRangeQuery(Node* node, K key1, K key2, std::map<K, V>* ret);

	//garbage collection:
	void retireNode(Node* state, Node* node);
	void retireState(Node* state, std::list<Node*>& retire_list_prev);
	void reclaimState(Node* state, std::list<Node*>& new_list);

	//memory tracker:
	RangeTracker<typename BonsaiTreeRange<K, V>::Node>* range_tracker;
	
	//retired node:
	static Node* retired_node;
	bool retiredNodeSpot(Node* ptr);


public:
	BonsaiTreeRange(GlobalTestConfig* gtc);
	~BonsaiTreeRange();

	unsigned long treeSize();

	optional<V> get(K key, int tid);
	optional<V> put(K key, V val, int tid);
	bool insert(K key, V val, int tid);
	optional<V> remove(K key, int tid);
	optional<V> replace(K key, V val, int tid);
	std::map<K, V> rangeQuery(K key1, K key2, int& len, int tid);
};


template <class K, class V> class BonsaiTreeRangeFactory : public RideableFactory{
	BonsaiTreeRange<K, V>* build(GlobalTestConfig* gtc){
		return new BonsaiTreeRange<K, V>(gtc);
	}
};

template<class K, class V>
__thread int BonsaiTreeRange<K, V>::local_tid;

template<class K, class V>
typename BonsaiTreeRange<K, V>::Node* BonsaiTreeRange<K, V>::retired_node = (BonsaiTreeRange<K, V>::Node*) 0x1;

#endif
