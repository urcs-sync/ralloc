#ifndef __QUEUE_H_
#define __QUEUE_H_

#include <stdint.h>
typedef struct {
	volatile __uint128_t top:64, ocount:64;
} top_aba_t;

// Pseudostructure for lock-free list elements.
// The only requirement is that the 5th-8th byte of
// each element should be available to be used as
// the pointer for the implementation of a singly-linked
// list. 
struct queue_elem_t {
	char 				*_dummy;
	volatile struct queue_elem_t 	*next;
};

typedef struct {
	uint64_t 	_pad0[8];
	top_aba_t	both;
	uint64_t 	_pad1[8];
} lf_fifo_queue_t;

#define LF_FIFO_QUEUE_STATIC_INIT	{{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}}

/******************************************************************************/

#endif

