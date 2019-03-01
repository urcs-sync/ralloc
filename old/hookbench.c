#include <stdio.h>
#include <stdlib.h>
#include "pmmalloc.h"
#include <pthread.h> 
#define p_malloc(sz) PM_malloc(sz)
#define p_free(ptr) PM_free(ptr)
// static void
// noop_alloc_hook(void *extra, hook_alloc_t type, void *result,
//     uintptr_t result_raw, uintptr_t args_raw[3]) {
// }

// static void
// noop_dalloc_hook(void *extra, hook_dalloc_t type, void *address,
//     uintptr_t args_raw[3]) {
// }

// static void
// noop_expand_hook(void *extra, hook_expand_t type, void *address,
//     size_t old_usize, size_t new_usize, uintptr_t result_raw,
//     uintptr_t args_raw[4]) {
// }
const int THREAD_NUM = 4;
void *malloc_free_loop(void *args) {
	int iters = *((int*)args);
	int **p=p_malloc(iters*sizeof(int*));
	int ret = 1;
	for (int i = 0; i < iters; i++) {
		p[i] = p_malloc(sizeof(int));
		*p[i] = i;
		ret=*p[i]+1;
		if (i%100000==0) fprintf(stderr,"%p: %d\n",p[i],*p[i]);
	}
	for(int i=0;i<iters;i++){
		p_free(p[i]);
	}
	printf("final: %d\n", ret);
	p_free(p);
	return NULL;
}

// static void
// test_hooked(int iters) {
// 	hooks_t hooks = {&noop_alloc_hook, &noop_dalloc_hook, &noop_expand_hook,
// 		NULL};

// 	int err;
// 	void *handles[HOOK_MAX];
// 	size_t sz = sizeof(handles[0]);

// 	for (int i = 0; i < HOOK_MAX; i++) {
// 		err = mallctl("experimental.hooks.install", &handles[i],
// 		    &sz, &hooks, sizeof(hooks));
// 		assert(err == 0);

// 		timedelta_t timer;
// 		timer_start(&timer);
// 		malloc_free_loop(iters);
// 		timer_stop(&timer);
// 		printf("With %d hook%s: %"FMTu64"us\n", i + 1,
// 		    i + 1 == 1 ? "" : "s", timer_usec(&timer));
// 	}
// 	for (int i = 0; i < HOOK_MAX; i++) {
// 		err = mallctl("experimental.hooks.remove", NULL, NULL,
// 		    &handles[i], sizeof(handles[i]));
// 		assert(err == 0);
// 	}
// }

static void
test_unhooked(int iters) {
	// timedelta_t timer;
	// timer_start(&timer);
	pthread_t thread_id[THREAD_NUM]; 
	for (int i = 0; i < THREAD_NUM; i++)
		pthread_create(&thread_id[i], NULL, malloc_free_loop, (void*)&iters); 
	for (int i = 0; i < THREAD_NUM; i++)
		pthread_join(thread_id[i], NULL); 
	// timer_stop(&timer);

	printf("Without hooks: done\n");
}

int
main(void) {
	/* Initialize */
	int iters = 10 * 1000 * 1000;
	printf("Benchmarking hooks with %d iterations:\n", iters);
	// test_hooked(iters);
	test_unhooked(iters);
}
