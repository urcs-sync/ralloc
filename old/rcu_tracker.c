/*
 * This is an implementation of rcu memory 
 * reclamation scheme
 *
 * The model is:
 *
 * Globally there are max_safe_epoch and current_epoch. 
 * current_epoch will be incremented when #operations for 
 * one thread reaches epoch_freq.
 *
 * Each thread has its local retire_list, reserve_epoch, and
 * op_counter, and reclamation will be attempted when 
 * op_counter reaches empty_freq.
 *
 * Every time when an operation starts, it reserves the current
 * epoch by updating its reservation[tid] to current_epoch.
 *
 * When a thread retires a block, it appends the block along with 
 * complete_snapshot to retire_list until op_counter reaches empty_freq. 
 * In that case, empty() will be called, which traverses the 
 * retire_list, and frees all entries with retire_epoch < max_safe_epoch.
 */


#include "rcu_tracker.h"
//allocate a list node from free list of region of list nodes
//or create a new region if the previous are full
static local_list_node* malloc_list_node(){//TODO
	return (local_list_node*) malloc(sizeof(local_list_node));
}

static void free_list_node(local_list_node* node){//TODO
	free(node);
}

static uint64_t* malloc_reserve_epoch(uint64_t thread_count_in){//TODO
	return (uint64_t*) malloc(sizeof(uint64_t)*thread_count_in);
}
//allocate a new region for list nodes
// static void* new_region_of_list_nodes(){//TODO
// 	void* addr;
// 	addr = mmap(NULL, sz + HEADER_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

// 	// If the highest bit of the descriptor is 1, then the object is large (allocated / freed directly from / to the OS)
// 	*((char*)addr) = (char)LARGE;
// 	addr += TYPE_SIZE;
// 	*((uint32_t *)addr) = sz + HEADER_SIZE;
// 	return (void*)(addr + PTR_SIZE); 
// }

static inline uint64_t min(uint64_t a, uint64_t b)
{
	return a < b ? a : b;
}

static inline uint64_t max(uint64_t a, uint64_t b)
{
	return a > b ? a : b;
}


//append a new node with ptr and retire_epoch to retire list
static void retire_list_append(void* ptr, uint64_t retire_epoch){
	//create
	local_list_node* new_node = (local_list_node*)malloc_list_node();
	new_node->next = NULL;
	new_node->ptr = ptr;
	new_node->retire_epoch = retire_epoch;
	//publish
	retire_list.tail->next = new_node;
	retire_list.tail = new_node;
}

//try to free all in the list with max_safe_epoch
static void retire_list_try_free_all(rcu_free_func_t free_func){
	uint64_t max_safe_epoch = UINT64_MAX;
	for(int i=0;j<thread_count;j++){
		max_safe_epoch = min(max_safe_epoch, atomic_load(&reserve_epoch[i]));
	}
	local_list_node* cur = retire_list.head.next;
	local_list_node* pre = &retire_list.head;
	while(cur != NULL){
		if(cur->retire_epoch < max_safe_epoch){
			free_func(cur->ptr);
			pre->next = cur->next;
			if(retire_list.tail == cur) retire_list.tail = pre;
			free_list_node(cur);
			cur = pre->next;
		} else{
			pre = cur;
			cur = cur->next;
		}
	}
}

void rcu_config_memory(){//TODO
	return;
}

void rcu_thread_register(){
	tid = atomic_fetch_add(&thread_count, 1);
	op_counter = 0;
}

//not thread safe, before rcu_thread_register()
void rcu_init(rcu_free_func_t free_func, uint64_t thread_count_in, uint64_t epoch_freq_in=150, uint64_t empty_freq_in=30){
#ifdef RCU_DEBUG
	initialized = true;
#endif
	rcu_config_memory();
	epoch_freq = epoch_freq_in;
	empty_freq = empty_freq_in;
	atomic_store(&epoch, 0);
	reserve_epoch = malloc_reserve_epoch(thread_count_in);
}

void rcu_retire(void* ptr){
	retire_list_append(ptr, atomic_load(&epoch));
	op_counter++;
	if(op_counter%epoch_freq == 0)
		atomic_fetch_add(&epoch, 1);
	if(retire_list.count%empty_freq == 0)
		retire_list_try_free_all();
}

inline void rcu_start_op(){
#ifdef RCU_DEBUG
	if(!initialized) assert(0&&"Initialize RCU first!");
#endif
	atomic_store(&reserve_epoch[tid], atomic_load(&epoch));
}

inline void rcu_end_op(){
	atomic_store(&reserve_epoch[tid], UINT64_MAX);
}