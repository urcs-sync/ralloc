/**
 * \file
 * Lock free queue.
 *
 * (C) Copyright 2011 Novell, Inc
 *
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


#ifndef LOCKFREEQUEUE_H
#define LOCKFREEQUEUE_H

//#define QUEUE_DEBUG	1

typedef struct _LockFreeQueueNode LockFreeQueueNode;

struct _LockFreeQueueNode {
	LockFreeQueueNode * volatile next;
#ifdef QUEUE_DEBUG
	int32_t in_queue;
#endif
};

typedef struct {
	LockFreeQueueNode node;
	volatile int32_t in_use;
} LockFreeQueueDummy;

#define LOCK_FREE_QUEUE_NUM_DUMMIES	2

typedef struct {
	LockFreeQueueNode * volatile head;
	LockFreeQueueNode * volatile tail;
	LockFreeQueueDummy dummies [LOCK_FREE_QUEUE_NUM_DUMMIES];
	volatile int32_t has_dummy;
} LockFreeQueue;

void lock_free_queue_init (LockFreeQueue *q);

void lock_free_queue_node_init (LockFreeQueueNode *node, int32_t poison);
void lock_free_queue_node_unpoison (LockFreeQueueNode *node);

void lock_free_queue_enqueue (LockFreeQueue *q, LockFreeQueueNode *node);

LockFreeQueueNode* lock_free_queue_dequeue (LockFreeQueue *q);

#endif
