/**
 * \file
 * Hazard pointer related code.
 *
 * (C) Copyright 2011 Novell, Inc
 * Licensed under the MIT license. See LICENSE file in the project root for full license information.
 */
#ifndef __HAZARD_POINTER_H__
#define __HAZARD_POINTER_H__

#include <stdatomic.h>

#define HAZARD_POINTER_COUNT 2

typedef struct {
	void* hazard_pointers [HAZARD_POINTER_COUNT];
} ThreadHazardPointers;

ThreadHazardPointers* hazard_pointer_get (void);
void* get_hazardous_pointer (void* volatile *pp, ThreadHazardPointers *hp, int hazard_index);

#define hazard_pointer_set(hp,i,v)	\
	do { assert ((i) >= 0 && (i) < HAZARD_POINTER_COUNT); \
		(hp)->hazard_pointers [(i)] = (v); \
		atomic_thread_fence(memory_order_acq_rel); \
	} while (0)

#define hazard_pointer_get_val(hp,i)	\
	((hp)->hazard_pointers [(i)])

#define hazard_pointer_clear(hp,i)	\
	do { assert ((i) >= 0 && (i) < HAZARD_POINTER_COUNT); \
		atomic_thread_fence(memory_order_acq_rel); \
		(hp)->hazard_pointers [(i)] = NULL; \
	} while (0)


void thread_small_id_free (int id);
int thread_small_id_alloc (void);

void thread_smr_init (void);
void thread_smr_cleanup (void);
#endif /*__HAZARD_POINTER_H__*/
