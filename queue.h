#ifndef __QUEUE_H_
#define __QUEUE_H_

#include <stdint.h>
typedef struct {
	__uint128_t ptr:64, ocount:64;
} aba_t;

// Pseudostructure for lock-free list elements.
// The only requirement is that the 1st-8th byte of
// each element should be available to be used as
// the pointer for the implementation of a singly-linked
// list. 
struct queue_elem_t {
	volatile aba_t next;
};

// Wentao: I don't understand why the original author put pad here. 
// I will just treat them as padding to avoid false-sharing so 
// I make them 32B in total.
typedef struct {
	volatile uint64_t has_dummy;
	struct queue_elem_t dummy;
	volatile aba_t		head;
	volatile aba_t 		tail;
	uint64_t 	_pad[2];
} lf_fifo_queue_t;

#define LF_FIFO_QUEUE_STATIC_INIT	{1, {{0, 0}}, {0, 0}, {0,0}, {0,0}}

/******************************************************************************/

#endif

