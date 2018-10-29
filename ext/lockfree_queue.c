/**
 * \file
 * Lock free queue.
 *
 * (C) Copyright 2011 Novell, Inc
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

/*
 * This is an implementation of a lock-free queue, as described in
 *
 * Simple, Fast, and Practical Non-Blocking and Blocking
 *   Concurrent Queue Algorithms
 * Maged M. Michael, Michael L. Scott
 * 1995
 *
 * A few slight modifications have been made:
 *
 * We use hazard pointers to rule out the ABA problem, instead of the
 * counter as in the paper.
 *
 * Memory management of the queue entries is done by the caller, not
 * by the queue implementation.  This implies that the dequeue
 * function must return the queue entry, not just the data.
 *
 * Therefore, the dummy entry must never be returned.  We do this by
 * re-enqueuing a new dummy entry after we dequeue one and then
 * retrying the dequeue.  We need more than one dummy because they
 * must be hazardly freed.
 */

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <rcu_tracker.h>
#include <lockfree_queue.h>
#include <stdatomic.h>

#define INVALID_NEXT	((LockFreeQueueNode *volatile)-1)
#define END_MARKER	((LockFreeQueueNode *volatile)-2)
#define FREE_NEXT	((LockFreeQueueNode *volatile)-3)

/*
 * Initialize a lock-free queue in-place at @q.
 */
void
lock_free_queue_init (LockFreeQueue *q)
{
	int i;
	for (i = 0; i < LOCK_FREE_QUEUE_NUM_DUMMIES; ++i) {
		q->dummies [i].node.next = (i == 0) ? END_MARKER : FREE_NEXT;
		q->dummies [i].in_use = i == 0 ? 1 : 0;
#ifdef QUEUE_DEBUG
		q->dummies [i].node.in_queue = i == 0 ? TRUE : FALSE;
#endif
	}

	q->head = q->tail = &q->dummies [0].node;
	q->has_dummy = 1;
	rcu_init();
}

/*
 * Initialize @node's state. If @poison is TRUE, @node may not be enqueued to a
 * queue - @lock_free_queue_node_unpoison must be called first; otherwise,
 * the node can be enqueued right away.
 *
 * The poisoning feature is mainly intended for ensuring correctness in complex
 * lock-free code that uses the queue. For example, in some code that reuses
 * nodes, nodes can be poisoned when they're dequeued, and then unpoisoned and
 * enqueued in their hazard free callback.
 */
void
lock_free_queue_node_init (LockFreeQueueNode *node, int32_t poison)
{
	node->next = poison ? INVALID_NEXT : FREE_NEXT;
#ifdef QUEUE_DEBUG
	node->in_queue = FALSE;
#endif
}

/*
 * Unpoisons @node so that it may be enqueued.
 */
void
lock_free_queue_node_unpoison (LockFreeQueueNode *node)
{
	assert (node->next == INVALID_NEXT);
#ifdef QUEUE_DEBUG
	assert (!node->in_queue);
#endif
	node->next = FREE_NEXT;
}

/*
 * Enqueue @node to @q. @node must have been initialized by a prior call to
 * @lock_free_queue_node_init, and must not be in a poisoned state.
 */
void
lock_free_queue_enqueue (LockFreeQueue *q, LockFreeQueueNode *node)
{
	ThreadHazardPointers *hp = hazard_pointer_get ();
	LockFreeQueueNode *tail;

#ifdef QUEUE_DEBUG
	assert (!node->in_queue);
	node->in_queue = TRUE;
	atomic_thread_fence(memory_order_acq_rel);//originally was write fence
#endif

	assert (node->next == FREE_NEXT);
	node->next = END_MARKER;
	for (;;) {
		LockFreeQueueNode *next;

		tail = (LockFreeQueueNode *) get_hazardous_pointer ((void* volatile*)&q->tail, hp, 0);//FIXME
		atomic_thread_fence(memory_order_acq_rel);//originally was read fence
		/*
		 * We never dereference next so we don't need a
		 * hazardous load.
		 */
		next = tail->next;
		atomic_thread_fence(memory_order_acq_rel);//originally was read fence

		/* Are tail and next consistent? */
		if (tail == q->tail) {
			assert (next != INVALID_NEXT && next != FREE_NEXT);
			assert (next != tail);

			if (next == END_MARKER) {
				/*
				 * Here we require that nodes that
				 * have been dequeued don't have
				 * next==END_MARKER.  If they did, we
				 * might append to a node that isn't
				 * in the queue anymore here.
				 */
				LockFreeQueueNode *tmp = node;//avoid changing node in CAS
				if (atomic_compare_exchange_weak ((void* volatile*)&tail->next, &tmp, END_MARKER) == END_MARKER)
					break;
			} else {
				/* Try to advance tail */
				atomic_compare_exchange_strong ((void* volatile*)&q->tail, &next, tail);
			}
		}
		hazard_pointer_clear (hp, 0);
	}

	/* Try to advance tail */
	atomic_compare_exchange_strong ((void* volatile*)&q->tail, &node, tail);
	hazard_pointer_clear (hp, 0);
}

static void
free_dummy (void* _dummy)
{
	LockFreeQueueDummy *dummy = (LockFreeQueueDummy *) _dummy;
	lock_free_queue_node_unpoison (&dummy->node);
	assert (dummy->in_use);
	atomic_thread_fence(memory_order_acq_rel);//originally was write fence
	dummy->in_use = 0;
}

static LockFreeQueueDummy*
get_dummy (LockFreeQueue *q)
{
	int i;
	for (i = 0; i < LOCK_FREE_QUEUE_NUM_DUMMIES; ++i) {
		LockFreeQueueDummy *dummy = &q->dummies [i];

		if (dummy->in_use)
			continue;
		int32_t tmp = 1;
		if (atomic_compare_exchange_strong (&dummy->in_use, &tmp, 0) == 0)
			return dummy;
	}
	return NULL;
}

static int32_t
is_dummy (LockFreeQueue *q, LockFreeQueueNode *n)
{
	return n >= &q->dummies [0].node && n <= &q->dummies [LOCK_FREE_QUEUE_NUM_DUMMIES-1].node;
}

static int32_t
try_reenqueue_dummy (LockFreeQueue *q)
{
	LockFreeQueueDummy *dummy;

	if (q->has_dummy)
		return FALSE;

	dummy = get_dummy (q);
	if (!dummy)
		return FALSE;

	int32_t tmp = 1;
	if (atomic_compare_exchange_strong (&q->has_dummy, &tmp, 0) != 0) {
		dummy->in_use = 0;
		return FALSE;
	}

	lock_free_queue_enqueue (q, &dummy->node);

	return TRUE;
}

/*
 * Dequeues a node from @q. Returns NULL if no nodes are available. The returned
 * node is hazardous and must be freed with @thread_hazardous_try_free or
 * @thread_hazardous_queue_free - it must not be freed directly.
 */
LockFreeQueueNode*
lock_free_queue_dequeue (LockFreeQueue *q)
{
	ThreadHazardPointers *hp = hazard_pointer_get ();//FIXME
	LockFreeQueueNode *head;

 retry:
	for (;;) {
		LockFreeQueueNode *tail, *next;

		head = (LockFreeQueueNode *) get_hazardous_pointer ((void* volatile*)&q->head, hp, 0);//FIXME
		tail = (LockFreeQueueNode*)q->tail;
		atomic_thread_fence(memory_order_acq_rel);//originally was read fence
		next = head->next;
		atomic_thread_fence(memory_order_acq_rel);//originally was read fence

		/* Are head, tail and next consistent? */
		if (head == q->head) {
			assert (next != INVALID_NEXT && next != FREE_NEXT);
			assert (next != head);

			/* Is queue empty or tail behind? */
			if (head == tail) {
				if (next == END_MARKER) {
					/* Queue is empty */
					hazard_pointer_clear (hp, 0);

					/*
					 * We only continue if we
					 * reenqueue the dummy
					 * ourselves, so as not to
					 * wait for threads that might
					 * not actually run.
					 */
					if (!is_dummy (q, head) && try_reenqueue_dummy (q))
						continue;

					return NULL;
				}

				/* Try to advance tail */
				atomic_compare_exchange_strong ((void* volatile*)&q->tail, &next, tail);
			} else {
				assert (next != END_MARKER);
				/* Try to dequeue head */
				if (atomic_compare_exchange_weak ((void* volatile*)&q->head, &next, head) == head)
					break;
			}
		}
		hazard_pointer_clear (hp, 0);
	}

	/*
	 * The head is dequeued now, so we know it's this thread's
	 * responsibility to free it - no other thread can.
	 */
	atomic_thread_fence(memory_order_acq_rel);//originally was write fence
	hazard_pointer_clear (hp, 0);

	assert (head->next);
	/*
	 * Setting next here isn't necessary for correctness, but we
	 * do it to make sure that we catch dereferencing next in a
	 * node that's not in the queue anymore.
	 */
	head->next = INVALID_NEXT;
#if QUEUE_DEBUG
	assert (head->in_queue);
	head->in_queue = FALSE;
	atomic_thread_fence(memory_order_acq_rel);//originally was write fence
#endif

	if (is_dummy (q, head)) {
		assert (q->has_dummy);
		q->has_dummy = 0;
		atomic_thread_fence(memory_order_acq_rel);//originally was write fence
		thread_hazardous_try_free (head, free_dummy);//FIXME
		if (try_reenqueue_dummy (q))
			goto retry;
		return NULL;
	}

	/* The caller must hazardously free the node. */
	return head;
}
