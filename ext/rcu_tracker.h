#ifndef RCUTRACKER_H
#define RCUTRACKER_H

#include <stdint.h>
#include <threads.h>
#include <stdbool.h>

#define RCU_DEBUG

#ifdef __cplusplus
extern "C" {
#endif

uint64_t epoch_freq = 150;
uint64_t empty_freq = 30;

typedef struct _local_list_node local_list_node;

struct _local_list_node{
	local_list_node* next;
	void* ptr;//the address of the block to free
	uint64_t retire_epoch;
}
typedef struct{
	local_list_node head = {NULL, NULL, 0};//dummy
	local_list_node* tail = &head;
} local_list;// thread local list for thread local use only

#ifdef RCU_DEBUG
static bool initialized = false;
#endif

static uint64_t epoch;
static uint64_t *reserve_epoch;//array in the size of thread count
thread_local int op_counter;//operation count
thread_local local_list retire_list;

typedef void (*rcu_free_func) (void* ptr);
void rcu_init();
void rcu_try_free_all();
void rcu_retire(void* ptr);
void rcu_start_op();
void rcu_end_op();

#ifdef __cplusplus
}
#endif

#endif// RCUTRACKER_H