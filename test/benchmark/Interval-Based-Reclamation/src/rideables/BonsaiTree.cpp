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


#include "BonsaiTree.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <list>
#include <map>

//#defiine LAZY_TRACKER

using namespace std;

/* routines under State */
template<class K, class V>
BonsaiTree<K, V>::State::State(MemoryTracker<Node>* tracker): 
	root(NULL), next(NULL){
		memory_tracker = tracker;
	}

template<class K, class V>
BonsaiTree<K, V>::State::State(BonsaiTree<K, V>::Node* r, BonsaiTree<K, V>::Node* n){
	root = r;
	next = n;
}

template<class K, class V>
BonsaiTree<K, V>::State::~State(){}

template<class K, class V>
void BonsaiTree<K, V>::State::addNewNode(BonsaiTree<K, V>::Node* new_node){
	new_list.push_back(new_node);
}


/* routines under BonsaiTree<K, V>::Node*/
template<class K, class V>
BonsaiTree<K, V>::Node::Node():left(NULL), right(NULL){
	size = 1;
}

template<class K, class V>
BonsaiTree<K, V>::Node::Node(BonsaiTree<K, V>::Node* l, BonsaiTree<K, V>::Node* r, K k, V v):
	left(l), right(r), key(k), value(v){
	unsigned long l_size = (l!=NULL)? l->size : 0;
	unsigned long r_size = (r!=NULL)? r->size : 0;
	size = l_size + r_size + 1;
}

template<class K, class V>
BonsaiTree<K, V>::Node::Node(State* state): state(state){}


template<class K, class V>
BonsaiTree<K, V>::Node::~Node() {
	if(state){
		delete state;
	}
}

/* routines under BonsaiTree */
template<class K, class V>
BonsaiTree<K, V>::BonsaiTree(GlobalTestConfig* gtc): RetiredMonitorable(gtc){
	std::string type = gtc->getEnv("tracker");
	if (type == "Hazard" || type == "HE") errexit("Hazard and HE not available ");
	int epochf = gtc->getEnv("epochf").empty()? 150:stoi(gtc->getEnv("epochf"));
	int emptyf = gtc->getEnv("emptyf").empty()? 30:stoi(gtc->getEnv("emptyf"));
	memory_tracker = new MemoryTracker<Node>(gtc, epochf, emptyf, 2, true);
	//initialize with an empty head state.
	local_tid = 0;
	curr_state.store(mkState());
}

template<class K, class V>
BonsaiTree<K, V>::~BonsaiTree(){}

/* State operations */
template<class K, class V>
typename BonsaiTree<K, V>::Node* BonsaiTree<K, V>::mkState(){
	void* ptr = memory_tracker->alloc(local_tid);
	return new (ptr) Node(new State(memory_tracker));
}

template<class K, class V>
typename BonsaiTree<K, V>::Node* BonsaiTree<K, V>::mkNode(BonsaiTree<K,V>::Node* state, 
	BonsaiTree<K, V>::Node* left, BonsaiTree<K, V>::Node* right, K key, V value){
	//Node* new_node = new Node(state->state->birth_epoch, left, right, key, value);
	if (retiredNodeSpot(left) || retiredNodeSpot(right)){
		return retired_node;
	}
	void* ptr = memory_tracker->alloc(local_tid);
	Node* new_node = new (ptr) Node(left, right, key, value);
	state->state->addNewNode(new_node);
	return new_node;
}

template<class K, class V>
void BonsaiTree<K, V>::retireNode(BonsaiTree<K, V>::Node* state, BonsaiTree<K, V>::Node* node){
       state->state->retire_list_prev.push_back(node);
}


template<class K, class V>
void BonsaiTree<K, V>::retireState(BonsaiTree<K, V>::Node* state, std::list<BonsaiTree<K, V>::Node*>& retire_list_prev){
	Node* node;

	for(; !retire_list_prev.empty(); retire_list_prev.pop_back()){
		assert(node!=retired_node);
		node = retire_list_prev.back();
		node->left = retired_node;
		node->right = retired_node;
		memory_tracker->retire(node, local_tid);
	}
	memory_tracker->retire(state, local_tid);
}

template<class K, class V>
void BonsaiTree<K, V>::reclaimState(BonsaiTree<K, V>::Node* state, std::list<BonsaiTree<K, V>::Node*>& new_list){
	Node* node;

	for(; !new_list.empty(); new_list.pop_back()){
		node = new_list.back();
		assert(node!=retired_node);
		assert(node->state==NULL);
		memory_tracker->reclaim(node, local_tid);
	}
	memory_tracker->reclaim(state, local_tid);
}

template<class K, class V>
unsigned long BonsaiTree<K, V>::nodeSize(BonsaiTree<K, V>::Node* node){
	assert(node != retired_node);
	return (node!=NULL)? node->size : 0;
}

template<class K, class V>
bool BonsaiTree<K, V>::retiredNodeSpot(BonsaiTree<K, V>::Node* ptr){
	if (ptr == retired_node){
		return true;
	}
	if (ptr){
		if (protect_read(ptr->left) == retired_node ||
			protect_read(ptr->right) == retired_node){
			return true;
		}
	}
	return false;
}

/* Tree operations */

template<class K, class V>
unsigned long BonsaiTree<K, V>::treeSize(){
	return nodeSize(curr_state.load()->state->root);
}

template<class K, class V>
typename BonsaiTree<K, V>::Node* BonsaiTree<K, V>::protect_read(atomic<BonsaiTree<K, V>::Node*>& node){
	return memory_tracker->read(node, 0, local_tid);
}

template<class K, class V>
optional<V> BonsaiTree<K, V>::update(Operation op, K key, V val, int tid){
	V* ori_val = NULL;
	optional<V> nf = {};
	optional<V> ret;
	bool ins_ret=false;

	Node* old_state;
	Node* new_state;
	Node* new_root;

	local_tid = tid;
	collect_retired_size(memory_tracker->get_retired_cnt(tid), tid);

#ifdef LAZY_TRACKER
	memory_tracker->start_op(tid);
#endif

	while(true){
		ori_val = NULL;
#ifndef LAZY_TRACKER
		memory_tracker->start_op(tid);
#endif
		old_state = memory_tracker->read(curr_state, 0, tid);
		new_state = mkState();
		switch(op){
			case op_put:
				new_state->state->root = doPut(new_state, protect_read(old_state->state->root), key, val, &ori_val);
				break;
			case op_replace:
				new_state->state->root = doReplace(new_state, protect_read(old_state->state->root), key, val, &ori_val);
				break;
			case op_remove:
				new_state->state->root = doRemove(new_state, protect_read(old_state->state->root), key, &ori_val);
				break;
			case op_insert:
				new_state->state->root = doInsert(new_state, protect_read(old_state->state->root), key, val, &ins_ret);
				ori_val = (ins_ret)? NULL : new V();
				break;
			default:
				assert(false && "operation type error.");
		}
		if (new_state->state->root == retired_node){
			if (ori_val) delete ori_val;
			reclaimState(new_state, new_state->state->new_list);
#ifndef LAZY_TRACKER
			memory_tracker->end_op(tid);
			memory_tracker->clear_all(tid);
#endif
			continue;
		}

		// memory_tracker->reserve(new_state, 1, tid);
		
		std::list<Node*> retire_list_prev = new_state->state->retire_list_prev;
		if (curr_state.compare_exchange_strong(old_state, new_state, 
			memory_order::memory_order_acq_rel, memory_order::memory_order_acquire)){
			retireState(old_state, retire_list_prev);
			//cout<<"retire state.";
			break;
		} else {
			if (ori_val) delete ori_val;
			//memory_tracker->template destruct<State>(new_state);
			// delete new_state;
			reclaimState(new_state, new_state->state->new_list);

#ifndef LAZY_TRACKER
			memory_tracker->end_op(tid);
			memory_tracker->clear_all(tid);
#endif
		}
	}
	memory_tracker->end_op(tid);
	memory_tracker->clear_all(tid);

	ret = (ori_val)? *ori_val : nf;
	if (ori_val){
		delete ori_val;
	}
	return ret;
}

template<class K, class V>
bool BonsaiTree<K, V>::insert(K key, V val, int tid){
	return (!update(op_insert, key, val, tid));
}

template<class K, class V>
optional<V> BonsaiTree<K, V>::put(K key, V val, int tid){
	return update(op_put, key, val, tid);
}

template<class K, class V>
optional<V> BonsaiTree<K, V>::replace(K key, V val, int tid){
	return update(op_replace, key, val, tid);
}

template<class K, class V>
optional<V> BonsaiTree<K, V>::remove(K key,int tid){
	return update(op_remove, key, V(), tid);
}

template<class K, class V>
optional<V> BonsaiTree<K, V>::get(K key, int tid){//TODO: new version needed.
	V ret;
	collect_retired_size(memory_tracker->get_retired_cnt(tid), tid);
	memory_tracker->start_op(tid);
	while(true){
		BonsaiTree<K, V>::Node* node = protect_read(protect_read(curr_state)->state->root);

		while (node && node != retired_node){
			if (node->key == key){
				break;
			} else if (key < node->key){
				node = protect_read(node->left);
			} else {
				node = protect_read(node->right);
			}
		}
		if (retiredNodeSpot(node)){
			continue;
		}
		if (node != NULL){
			ret = node->value;//NOTE: careful. copy constructor needed here.
			memory_tracker->end_op(tid);
			memory_tracker->clear_all(tid);
			return ret;
		} else {
			memory_tracker->end_op(tid);
			memory_tracker->clear_all(tid);
			return V();
		}
	}
}

template<class K, class V>
map<K, V> BonsaiTree<K,V>::rangeQuery(K key1, K key2, int& len, int tid){
	map<K, V> ret;
	memory_tracker->start_op(tid);
	BonsaiTree<K, V>::Node* root = curr_state.load()->state->root;
	doRangeQuery(root, key1, key2, &ret);
	memory_tracker->clear_all(tid);
	memory_tracker->end_op(tid);
	return ret;
}

template<class K, class V>
typename BonsaiTree<K, V>::Node* BonsaiTree<K, V>::doInsert(BonsaiTree<K, V>::Node* state, 
	BonsaiTree<K, V>::Node* node, K key, V value, bool *ret){
	if (!node){
		*ret = true; //return true, insert successful.
		return mkNode(state, NULL, NULL, key, value);
	}
	if (retiredNodeSpot(node)){
		return retired_node;
	}
	if (key == node->key){
		*ret = false; //return false, insert failed.
		return node;
	} else if (key < node->key){
		return mkBalanced(state, node, doInsert(state, protect_read(node->left), key, value, ret),
			protect_read(node->right));
	} else {//if (key > node->key){
		return mkBalanced(state, node, protect_read(node->left), 
			doInsert(state, protect_read(node->right), key, value, ret));
	}
}

template<class K, class V>
typename BonsaiTree<K, V>::Node* BonsaiTree<K, V>::doPut(BonsaiTree<K, V>::Node* state, 
	BonsaiTree<K, V>::Node* node, K key, V value, V **ori_val){
	if (!node){
		*ori_val = NULL;
		return mkNode(state, NULL, NULL, key, value);
	}
	if (retiredNodeSpot(node)){
		return retired_node;
	}
	if (key == node->key){
		*ori_val = new V(node->value);
		retireNode(state, node);
		return mkNode(state, protect_read(node->left), protect_read(node->right), key, value);
	} else if (key < node->key){
		return mkBalanced(state, node, doPut(state, protect_read(node->left), key, value, ori_val),
			protect_read(node->right));
	} else {//if (key > node->key){
		return mkBalanced(state, node, protect_read(node->left), 
			doPut(state, protect_read(node->right), key, value, ori_val));
	}	
}

template<class K, class V>
typename BonsaiTree<K, V>::Node* BonsaiTree<K, V>::doReplace(BonsaiTree<K, V>::Node* state, 
	BonsaiTree<K, V>::Node* node, K key, V value, V **ori_val){
	if (!node){
		//*ori_val = new V();//return empty string.
		*ori_val = NULL;
		return NULL;//if no ori_val, don't insert anything.
	} else if (retiredNodeSpot(node)){
		return retired_node;
	}
	if (key == node->key){
		*ori_val = new V(node->value);
		retireNode(state, node);
		return mkNode(state, protect_read(node->left), protect_read(node->right), key, value);
	} else if (key < node->key){
		return mkBalanced(state, node, doReplace(state, protect_read(node->left), key, value, ori_val),
			protect_read(node->right));
	} else {//if (key > node->key){
		return mkBalanced(state, node, protect_read(node->left), 
			doReplace(state, protect_read(node->right), key, value, ori_val));
	}
}

template<class K, class V>
typename BonsaiTree<K, V>::Node* BonsaiTree<K, V>::doRemove(BonsaiTree<K, V>::Node* state, 
	BonsaiTree<K, V>::Node* node, K key, V **ori_val){
	if (!node){
		//*ori_val = new V();//return empty string.
		*ori_val = NULL;
		return NULL;
	} else if (retiredNodeSpot(node)){
		return retired_node;
	} 
	if (key == node->key){
		*ori_val = new V(node->value);
		if (node->size == 1){ //it's a leaf node
			retireNode(state, node);
			return NULL;
		} else {
			retireNode(state, node);
			BonsaiTree<K, V>::Node* successor = NULL;
			if (node->left) {
				BonsaiTree<K, V>::Node* new_left = pullRightMost(state, protect_read(node->left), &successor);
				assert(successor!=NULL);
				return mkBalanced(state, successor, new_left, protect_read(node->right));
			} else {//if (node->right) {
				BonsaiTree<K, V>::Node* new_right = pullLeftMost(state, protect_read(node->right), &successor);
				assert(successor!=NULL);
				return mkBalanced(state, successor, protect_read(node->left), new_right);
			} 
		}
	} else if (key < node->key){
		return mkBalanced(state, node, doRemove(state, protect_read(node->left), key, ori_val),
			protect_read(node->right));
	} else {//if (key > node->key){
		return mkBalanced(state, node, protect_read(node->left), 
			doRemove(state, protect_read(node->right), key, ori_val));
	}
}

template<class K, class V>
typename BonsaiTree<K, V>::Node* BonsaiTree<K, V>::pullLeftMost(BonsaiTree<K, V>::Node* state, 
	BonsaiTree<K, V>::Node* node, BonsaiTree<K, V>::Node** successor){
	if (retiredNodeSpot(node)){
		*successor = retired_node;
		return retired_node;
	}
	if (node->left){
		return mkBalanced(state, node, pullLeftMost(state, protect_read(node->left), successor), protect_read(node->right));
	} else {//node is the leftmost node.
		*successor = mkNode(state, NULL, NULL, node->key, node->value);
		retireNode(state, node);
		return protect_read(node->right); 
	}
}

template<class K, class V>
typename BonsaiTree<K, V>::Node* BonsaiTree<K, V>::pullRightMost(BonsaiTree<K, V>::Node* state, 
	BonsaiTree<K, V>::Node* node, BonsaiTree<K, V>::Node** successor){
	if (retiredNodeSpot(node)){
		*successor = retired_node;
		return retired_node;
	}
	if (node->right){
		return mkBalanced(state, node, protect_read(node->left), pullRightMost(state, protect_read(node->right), successor));
	} else {//node is the rightmost node.
		*successor = mkNode(state, NULL, NULL, node->key, node->value);
		retireNode(state, node);
		return protect_read(node->left);
	}
}

template<class K, class V>
typename BonsaiTree<K, V>::Node* BonsaiTree<K, V>::mkBalanced(BonsaiTree<K, V>::Node* state, 
	BonsaiTree<K, V>::Node* node, BonsaiTree<K, V>::Node* left, BonsaiTree<K, V>::Node* right){
	if (retiredNodeSpot(node) || retiredNodeSpot(left) || retiredNodeSpot(right)){
		return retired_node;
	}
	unsigned long l_size = nodeSize(left);
	unsigned long r_size = nodeSize(right);
	V value = node->value;
	K key = node->key;
	BonsaiTree<K, V>::Node* out;

	//made a patch to the original psudocode, the commented ones.
	//if(l_size && r_size && r_size > WEIGHT*l_size){
	if(r_size && ((l_size && r_size > WEIGHT*l_size) || (!l_size && r_size > WEIGHT))){
		out = mkBalancedL(state, left, right, key, value);
	//} else if (l_size && r_size && l_size > WEIGHT*r_size){
	} else if (l_size && ((r_size && l_size > WEIGHT*r_size) || (!r_size && l_size > WEIGHT))){
		out = mkBalancedR(state, left, right, key, value);
	} else {
		out = mkNode(state, left, right, key, value);
	}
	retireNode(state, node);
	return out;
}

template<class K, class V>
typename BonsaiTree<K, V>::Node* BonsaiTree<K, V>::mkBalancedL(BonsaiTree<K, V>::Node* state, 
	BonsaiTree<K, V>::Node* left, BonsaiTree<K, V>::Node* right, K key, V value){
	assert(right!=NULL);
	BonsaiTree<K, V>::Node* out;
	Node* right_left = protect_read(right->left);
	Node* right_right = protect_read(right->right);
	if (retiredNodeSpot(right_left) || retiredNodeSpot(right_right)){
		return retired_node;
	}
	if (nodeSize(right_left) < nodeSize(right_right)){
		return singleL(state, left, right, right_left, right_right, key, value);
	} else {
		return doubleL(state, left, right, right_left, right_right, key, value);
	}
}

template<class K, class V>
typename BonsaiTree<K, V>::Node* BonsaiTree<K, V>::mkBalancedR(BonsaiTree<K, V>::Node* state, 
	BonsaiTree<K, V>::Node* left, BonsaiTree<K, V>::Node* right, K key, V value){
	assert(left!=NULL);
	Node* left_right = protect_read(left->right);
	Node* left_left = protect_read(left->left);
	if (retiredNodeSpot(left_right) || retiredNodeSpot(left_left)){
		return retired_node;
	}
	if (nodeSize(left_right) < nodeSize(left_left)){
		return singleR(state, left, right, left_right, left_left, key, value);
	} else {
		return doubleR(state, left, right, left_right, left_left, key, value);
	}
}

template<class K, class V>
typename BonsaiTree<K, V>::Node* BonsaiTree<K, V>::singleL(BonsaiTree<K, V>::Node* state, 
	BonsaiTree<K, V>::Node* left, BonsaiTree<K, V>::Node* right,
	BonsaiTree<K, V>::Node* right_left, BonsaiTree<K, V>::Node* right_right, K key, V value){
	assert(right!=NULL);
	BonsaiTree<K, V>::Node* out = mkNode(state, mkNode(state, left, right_left, key, value), 
		right_right, right->key, right->value);
	retireNode(state, right);
	return out;
}

template<class K, class V>
typename BonsaiTree<K, V>::Node* BonsaiTree<K, V>::doubleL(BonsaiTree<K, V>::Node* state, 
	BonsaiTree<K, V>::Node* left, BonsaiTree<K, V>::Node* right,
	BonsaiTree<K, V>::Node* right_left, BonsaiTree<K, V>::Node* right_right, K key, V value){
	
	BonsaiTree<K, V>::Node* out = mkNode(state, 
		mkNode(state, left, protect_read(right_left->left), key, value),
		mkNode(state, protect_read(right_left->right), right_right, right->key, right->value),
		right_left->key, right_left->value);
	retireNode(state, right_left);
	retireNode(state, right);
	return out;
}

template<class K, class V>
typename BonsaiTree<K, V>::Node* BonsaiTree<K, V>::singleR(BonsaiTree<K, V>::Node* state, 
	BonsaiTree<K, V>::Node* left, BonsaiTree<K, V>::Node* right,
	BonsaiTree<K, V>::Node* left_right, BonsaiTree<K, V>::Node* left_left, K key, V value){
	assert(left!=NULL);
	BonsaiTree<K, V>::Node* out = mkNode(state, left_left,
		mkNode(state, left_right, right, key, value), left->key, left->value);
	retireNode(state, left);
	return out;
}

template<class K, class V>
typename BonsaiTree<K, V>::Node* BonsaiTree<K, V>::doubleR(BonsaiTree<K, V>::Node* state, 
	BonsaiTree<K, V>::Node* left, BonsaiTree<K, V>::Node* right,
	BonsaiTree<K, V>::Node* left_right, BonsaiTree<K, V>::Node* left_left, K key, V value){
	
	BonsaiTree<K, V>::Node* out = mkNode(state, 
		mkNode(state, left_left, protect_read(left_right->left), left->key, left->value),
		mkNode(state, protect_read(left_right->right), right, key, value),
		left_right->key, left_right->value);

	assert(left_right!=left);
	retireNode(state, left_right);
	retireNode(state, left);
	return out;
}

template<class K, class V>
void BonsaiTree<K,V>::doRangeQuery(Node* node, K key1, K key2, std::map<K, V>* ret){
	// TODO: test this.
	// if (node->value > key1 && node->left){
	// 	doRangeQuery(node->left, key1, key2, ret);
	// }
	// if (node->value >= key1 && node->value <= key2){
	// 	ret->emplace(node->key, node->value);
	// }
	// if (node->value < key2 && node->right){
	// 	doRangeQuery(node->right, key1, key2, ret);
	// }
}

template class BonsaiTree<std::string, std::string>;
template class BonsaiTree<int, int>;
