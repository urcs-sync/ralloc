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


#include "BonsaiTreeRange.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <list>
#include <map>

using namespace std;

//#define LAZY_TRACKER

/* routines under State */
template<class K, class V>
BonsaiTreeRange<K, V>::State::State(RangeTracker<Node>* tracker): 
	root(NULL), next(NULL){
		range_tracker = tracker;
	}

template<class K, class V>
BonsaiTreeRange<K, V>::State::State(BonsaiTreeRange<K, V>::Node* r, BonsaiTreeRange<K, V>::Node* n){
	//might not be useful.
	root = r;
	next = n;
}

template<class K, class V>
BonsaiTreeRange<K, V>::State::~State(){}

template<class K, class V>
void BonsaiTreeRange<K, V>::State::addNewNode(BonsaiTreeRange<K, V>::Node* new_node){
	new_list.push_back(new_node);
}


/* routines under BonsaiTreeRange<K, V>::Node*/
template<class K, class V>
BonsaiTreeRange<K, V>::Node::Node():left(NULL), right(NULL){
	//might not be useful. just in case.
	size = 1;
}

template<class K, class V>
BonsaiTreeRange<K, V>::Node::Node(BonsaiTreeRange<K, V>::Node* l, BonsaiTreeRange<K, V>::Node* r, K k, V v):
	left(l), right(r), key(k), value(v){
	unsigned long l_size = (l!=NULL)? l->size : 0;
	unsigned long r_size = (r!=NULL)? r->size : 0;
	size = l_size + r_size + 1;
}

template<class K, class V>
BonsaiTreeRange<K, V>::Node::Node(State* state): state(state){}


template<class K, class V>
BonsaiTreeRange<K, V>::Node::~Node() {
	if(state){
		delete state;
	}
}

/* routines under BonsaiTreeRange */

template<class K, class V>
BonsaiTreeRange<K, V>::BonsaiTreeRange(GlobalTestConfig* gtc): RetiredMonitorable(gtc){
	//range_tracker = new RangeTracker<BonsaiTreeRange<K, V>::Node>(gtc->task_num, 150, 200, true);
	std::string type = gtc->getEnv("tracker").empty()? "LF":gtc->getEnv("tracker");
	if (type == "Hazard" || type == "HP") errexit("Use _dynamic versions instead.");
	int epochf = gtc->getEnv("epochf").empty()? 150:stoi(gtc->getEnv("epochf"));
	int emptyf = gtc->getEnv("emptyf").empty()? 30:stoi(gtc->getEnv("emptyf"));
	range_tracker = new RangeTracker<Node>(gtc, epochf, emptyf, true);
	//initialize with an empty head state.
	//curr_state.store(new State());
	local_tid = 0;
	curr_state.store(mkState());
}

template<class K, class V>
BonsaiTreeRange<K, V>::~BonsaiTreeRange(){}

/* State operations */
template<class K, class V>
typename BonsaiTreeRange<K, V>::Node* BonsaiTreeRange<K, V>::mkState(){
	void* ptr = range_tracker->alloc(local_tid);
	return new (ptr) Node(new State(range_tracker));
}


template<class K, class V>
typename BonsaiTreeRange<K, V>::Node* BonsaiTreeRange<K, V>::mkNode(BonsaiTreeRange<K,V>::Node* state, 
	BonsaiTreeRange<K, V>::Node* left, BonsaiTreeRange<K, V>::Node* right, K key, V value){
	if (retiredNodeSpot(left) || retiredNodeSpot(right)){
		return retired_node;
	}
	void* ptr = range_tracker->alloc(local_tid);
	Node* new_node = new (ptr) Node(left, right, key, value);
	state->state->addNewNode(new_node);
	return new_node;
}

template<class K, class V>
void BonsaiTreeRange<K, V>::retireNode(BonsaiTreeRange<K, V>::Node* state, BonsaiTreeRange<K, V>::Node* node){
       state->state->retire_list_prev.push_back(node);
}


template<class K, class V>
void BonsaiTreeRange<K, V>::retireState(BonsaiTreeRange<K, V>::Node* state, std::list<BonsaiTreeRange<K, V>::Node*>& retire_list_prev){
	Node* node;

	for(; !retire_list_prev.empty(); retire_list_prev.pop_back()){
		assert(node!=retired_node);
		node = retire_list_prev.back();
		node->left = retired_node;
		node->right = retired_node;
		range_tracker->retire(node, local_tid);
	}
	range_tracker->retire(state, local_tid);
}

template<class K, class V>
void BonsaiTreeRange<K, V>::reclaimState(BonsaiTreeRange<K, V>::Node* state, std::list<BonsaiTreeRange<K, V>::Node*>& new_list){
	Node* node;

	for(; !new_list.empty(); new_list.pop_back()){
		node = new_list.back();
		assert(node!=retired_node);
		assert(node->state==NULL);
		range_tracker->reclaim(node);
	}
	range_tracker->reclaim(state);
}

template<class K, class V>
unsigned long BonsaiTreeRange<K, V>::nodeSize(BonsaiTreeRange<K, V>::Node* node){
	assert(node != retired_node);
	return (node!=NULL)? node->size : 0;
}

template<class K, class V>
bool BonsaiTreeRange<K, V>::retiredNodeSpot(BonsaiTreeRange<K, V>::Node* ptr){
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
unsigned long BonsaiTreeRange<K, V>::treeSize(){
	return nodeSize(curr_state.load()->state->root.ptr());
}

template<class K, class V>
typename BonsaiTreeRange<K, V>::Node* BonsaiTreeRange<K, V>::protect_read(biptr<BonsaiTreeRange<K, V>::Node>& node){
	return range_tracker->read(node);
}

template<class K, class V>
optional<V> BonsaiTreeRange<K, V>::update(Operation op, K key, V val, int tid){
	V* ori_val = NULL;
	optional<V> nf = {};
	optional<V> ret;
	bool ins_ret=false;

	Node* old_state;
	Node* new_state;
	Node* new_root;

	local_tid = tid;
	collect_retired_size(range_tracker->get_retired_cnt(tid), tid);

#ifdef LAZY_TRACKER
	range_tracker->reserve(tid);
#endif

	while(true){
		ori_val = NULL;
#ifndef LAZY_TRACKER
		range_tracker->reserve(tid);
#endif
		old_state = range_tracker->read(curr_state);
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
			range_tracker->clear(tid);
#endif
			continue;
		}

		
		std::list<Node*> retire_list_prev = new_state->state->retire_list_prev;
		if (curr_state.CAS(old_state, new_state, memory_order::memory_order_acq_rel)){
			retireState(old_state, retire_list_prev);
			break;
		} else {
			if (ori_val) delete ori_val;
			reclaimState(new_state, new_state->state->new_list);

#ifndef LAZY_TRACKER
			range_tracker->clear(tid);
#endif
		}
	}
	range_tracker->clear(tid);

	ret = (ori_val)? *ori_val : nf;
	if (ori_val){
		delete ori_val;
	}
	return ret;
}

template<class K, class V>
bool BonsaiTreeRange<K, V>::insert(K key, V val, int tid){
	return (!update(op_insert, key, val, tid));
}

template<class K, class V>
optional<V> BonsaiTreeRange<K, V>::put(K key, V val, int tid){
	return update(op_put, key, val, tid);
}

template<class K, class V>
optional<V> BonsaiTreeRange<K, V>::replace(K key, V val, int tid){
	return update(op_replace, key, val, tid);
}

template<class K, class V>
optional<V> BonsaiTreeRange<K, V>::remove(K key,int tid){
	return update(op_remove, key, V(), tid);
}

template<class K, class V>
optional<V> BonsaiTreeRange<K, V>::get(K key, int tid){//TODO: new version needed.
	V ret;
	collect_retired_size(range_tracker->get_retired_cnt(tid), tid);
	range_tracker->reserve(tid);
	while(true){
		BonsaiTreeRange<K, V>::Node* node = protect_read(protect_read(curr_state)->state->root);

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
			range_tracker->clear(tid);
			return ret;
		} else {
			range_tracker->clear(tid);
			return V();
		}
	}
}

template<class K, class V>
map<K, V> BonsaiTreeRange<K,V>::rangeQuery(K key1, K key2, int& len, int tid){//TODO: new version needed.
	map<K, V> ret;
	range_tracker->reserve(tid);
	BonsaiTreeRange<K, V>::Node* root = curr_state->state->root.ptr();
	doRangeQuery(root, key1, key2, &ret);
	range_tracker->clear(tid);
	return ret;
}

template<class K, class V>
typename BonsaiTreeRange<K, V>::Node* BonsaiTreeRange<K, V>::doInsert(BonsaiTreeRange<K, V>::Node* state, 
	BonsaiTreeRange<K, V>::Node* node, K key, V value, bool *ret){
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
typename BonsaiTreeRange<K, V>::Node* BonsaiTreeRange<K, V>::doPut(BonsaiTreeRange<K, V>::Node* state, 
	BonsaiTreeRange<K, V>::Node* node, K key, V value, V **ori_val){
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
typename BonsaiTreeRange<K, V>::Node* BonsaiTreeRange<K, V>::doReplace(BonsaiTreeRange<K, V>::Node* state, 
	BonsaiTreeRange<K, V>::Node* node, K key, V value, V **ori_val){
	if (!node){
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
typename BonsaiTreeRange<K, V>::Node* BonsaiTreeRange<K, V>::doRemove(BonsaiTreeRange<K, V>::Node* state, 
	BonsaiTreeRange<K, V>::Node* node, K key, V **ori_val){
	if (!node){
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
			BonsaiTreeRange<K, V>::Node* successor = NULL;
			if (node->left) {
				BonsaiTreeRange<K, V>::Node* new_left = pullRightMost(state, protect_read(node->left), &successor);
				assert(successor!=NULL);
				return mkBalanced(state, successor, new_left, protect_read(node->right));
			} else {//if (node->right) {
				BonsaiTreeRange<K, V>::Node* new_right = pullLeftMost(state, protect_read(node->right), &successor);
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
typename BonsaiTreeRange<K, V>::Node* BonsaiTreeRange<K, V>::pullLeftMost(BonsaiTreeRange<K, V>::Node* state, 
	BonsaiTreeRange<K, V>::Node* node, BonsaiTreeRange<K, V>::Node** successor){
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
typename BonsaiTreeRange<K, V>::Node* BonsaiTreeRange<K, V>::pullRightMost(BonsaiTreeRange<K, V>::Node* state, 
	BonsaiTreeRange<K, V>::Node* node, BonsaiTreeRange<K, V>::Node** successor){
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
typename BonsaiTreeRange<K, V>::Node* BonsaiTreeRange<K, V>::mkBalanced(BonsaiTreeRange<K, V>::Node* state, 
	BonsaiTreeRange<K, V>::Node* node, BonsaiTreeRange<K, V>::Node* left, BonsaiTreeRange<K, V>::Node* right){
	if (retiredNodeSpot(node) || retiredNodeSpot(left) || retiredNodeSpot(right)){
		return retired_node;
	}
	unsigned long l_size = nodeSize(left);
	unsigned long r_size = nodeSize(right);
	V value = node->value;
	K key = node->key;
	BonsaiTreeRange<K, V>::Node* out;

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
typename BonsaiTreeRange<K, V>::Node* BonsaiTreeRange<K, V>::mkBalancedL(BonsaiTreeRange<K, V>::Node* state, 
	BonsaiTreeRange<K, V>::Node* left, BonsaiTreeRange<K, V>::Node* right, K key, V value){
	assert(right!=NULL);
	BonsaiTreeRange<K, V>::Node* out;
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
typename BonsaiTreeRange<K, V>::Node* BonsaiTreeRange<K, V>::mkBalancedR(BonsaiTreeRange<K, V>::Node* state, 
	BonsaiTreeRange<K, V>::Node* left, BonsaiTreeRange<K, V>::Node* right, K key, V value){
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
typename BonsaiTreeRange<K, V>::Node* BonsaiTreeRange<K, V>::singleL(BonsaiTreeRange<K, V>::Node* state, 
	BonsaiTreeRange<K, V>::Node* left, BonsaiTreeRange<K, V>::Node* right,
	BonsaiTreeRange<K, V>::Node* right_left, BonsaiTreeRange<K, V>::Node* right_right, K key, V value){
	assert(right!=NULL);
	BonsaiTreeRange<K, V>::Node* out = mkNode(state, mkNode(state, left, right_left, key, value), 
		right_right, right->key, right->value);
	retireNode(state, right);
	return out;
}

template<class K, class V>
typename BonsaiTreeRange<K, V>::Node* BonsaiTreeRange<K, V>::doubleL(BonsaiTreeRange<K, V>::Node* state, 
	BonsaiTreeRange<K, V>::Node* left, BonsaiTreeRange<K, V>::Node* right,
	BonsaiTreeRange<K, V>::Node* right_left, BonsaiTreeRange<K, V>::Node* right_right, K key, V value){
	
	BonsaiTreeRange<K, V>::Node* out = mkNode(state, 
		mkNode(state, left, protect_read(right_left->left), key, value),
		mkNode(state, protect_read(right_left->right), right_right, right->key, right->value),
		right_left->key, right_left->value);
	retireNode(state, right_left);
	retireNode(state, right);
	return out;
}

template<class K, class V>
typename BonsaiTreeRange<K, V>::Node* BonsaiTreeRange<K, V>::singleR(BonsaiTreeRange<K, V>::Node* state, 
	BonsaiTreeRange<K, V>::Node* left, BonsaiTreeRange<K, V>::Node* right,
	BonsaiTreeRange<K, V>::Node* left_right, BonsaiTreeRange<K, V>::Node* left_left, K key, V value){
	assert(left!=NULL);
	BonsaiTreeRange<K, V>::Node* out = mkNode(state, left_left,
		mkNode(state, left_right, right, key, value), left->key, left->value);
	retireNode(state, left);
	return out;
}

template<class K, class V>
typename BonsaiTreeRange<K, V>::Node* BonsaiTreeRange<K, V>::doubleR(BonsaiTreeRange<K, V>::Node* state, 
	BonsaiTreeRange<K, V>::Node* left, BonsaiTreeRange<K, V>::Node* right,
	BonsaiTreeRange<K, V>::Node* left_right, BonsaiTreeRange<K, V>::Node* left_left, K key, V value){
	
	BonsaiTreeRange<K, V>::Node* out = mkNode(state, 
		mkNode(state, left_left, protect_read(left_right->left), left->key, left->value),
		mkNode(state, protect_read(left_right->right), right, key, value),
		left_right->key, left_right->value);

	assert(left_right!=left);
	retireNode(state, left_right);
	retireNode(state, left);
	return out;
}


template<class K, class V>
void BonsaiTreeRange<K,V>::doRangeQuery(Node* node, K key1, K key2, std::map<K, V>* ret){
	//TODO: test this.
	/*if (node->value > key1 && node->left){
		doRangeQuery(node->left, key1, key2, ret);
	}
	if (node->value >= key1 && node->value <= key2){
		ret->emplace(node->key, node->value);
	}
	if (node->value < key2 && node->right){
		doRangeQuery(node->right, key1, key2, ret);
	}*/
}

template class BonsaiTreeRange<std::string, std::string>;
template class BonsaiTreeRange<int, int>;
