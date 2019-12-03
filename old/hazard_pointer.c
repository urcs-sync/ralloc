/**
 * \file
 * Hazard pointer related code.
 *
 * (C) Copyright 2011 Novell, Inc
 * Licensed under the MIT license. See LICENSE file in the project root for full license information.
 */

#include <config.h>
#include <string.h>

#include "hazard_pointer.h"

typedef struct {
	void* p;
	HazardousFreeFunc free_func;
} DelayedFreeItem;

/* The hazard table */
#if SMALL_CONFIG
#define HAZARD_TABLE_MAX_SIZE	256
#define HAZARD_TABLE_OVERFLOW	4
#else
#define HAZARD_TABLE_MAX_SIZE	16384 /* There cannot be more threads than this number. */
#define HAZARD_TABLE_OVERFLOW	64
#endif

static volatile int hazard_table_size = 0;
static ThreadHazardPointers * volatile hazard_table = NULL;

static uint64_t
is_pointer_hazardous (void* p)
{
	int i, j;
	int highest = highest_small_id;

	assert (highest < hazard_table_size);

	for (i = 0; i <= highest; ++i) {
		for (j = 0; j < HAZARD_POINTER_COUNT; ++j) {
			if (hazard_table [i].hazard_pointers [j] == p)
				return 1;
			LOAD_LOAD_FENCE;
		}
	}

	return 0;
}

ThreadHazardPointers*
hazard_pointer_get (void)
{
	int small_id = thread_info_get_small_id ();

	if (small_id < 0) {
		static ThreadHazardPointers emerg_hazard_table;
		g_warning ("Thread %p may have been prematurely finalized", (void*) (gsize) native_thread_id_get ());
		return &emerg_hazard_table;
	}

	return &hazard_table [small_id];
}

/* Can be called with hp==NULL, in which case it acts as an ordinary
   pointer fetch.  It's used that way indirectly from
   jit_info_table_add(), which doesn't have to care about hazards
   because it holds the respective domain lock. */
void*
get_hazardous_pointer (void* volatile *pp, ThreadHazardPointers *hp, int hazard_index)
{
	void* p;

	for (;;) {
		/* Get the pointer */
		p = *pp;
		/* If we don't have hazard pointers just return the
		   pointer. */
		if (!hp)
			return p;
		/* Make it hazardous */
		hazard_pointer_set (hp, hazard_index, p);
		/* Check that it's still the same.  If not, try
		   again. */
		if (*pp != p) {
			hazard_pointer_clear (hp, hazard_index);
			continue;
		}
		break;
	}

	return p;
}

void
thread_smr_init (void)
{
	int i;

	os_mutex_init_recursive(&small_id_mutex);
	counters_register ("Hazardous pointers", COUNTER_JIT | COUNTER_INT, &hazardous_pointer_count);

	for (i = 0; i < HAZARD_TABLE_OVERFLOW; ++i) {
		int small_id = thread_small_id_alloc ();
		assert (small_id == i);
	}
}

void
thread_smr_cleanup (void)
{
	thread_hazardous_try_free_all ();

	lock_free_array_queue_cleanup (&delayed_free_queue);

	/*FIXME, can't we release the small id table here?*/
}
