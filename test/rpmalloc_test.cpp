#include <cstdio>
#include <iostream>
#include <pthread.h>
#include <atomic>

#include "ralloc.hpp"
// #include "thread_util.hpp"

using namespace std;

const int THREAD_NUM = 4;
// ralloc* alloc;
atomic<bool> start{false};

void *malloc_free_loop(void *args) {
	while(!start.load());
	int iters = *((int*)args);
	int **p = RP_malloc(iters*sizeof(int*));
	int ret = 1;
	for (int i = 0; i < iters; i++) {
		p[i] = RP_malloc(sizeof(int));
		*p[i] = i;
		ret=*p[i]+1;
		if (i%100000==0) fprintf(stderr,"%p: %d\n",p[i],*p[i]);
	}
	for(int i=0;i<iters;i++){
		RP_free(p[i]);
	}
	printf("final: %d\n", ret);
	RP_free(p);
	return nullptr;
}
static void
test_unhooked(int iters) {
	// timedelta_t timer;
	// timer_start(&timer);
	pthread_t thread_id[THREAD_NUM];
	for (int i = 0; i < THREAD_NUM; i++)
		RP_pthread_create(&thread_id[i], NULL, malloc_free_loop, (void*)&iters); 
	start.store(true);
	for (int i = 0; i < THREAD_NUM; i++)
		pthread_join(thread_id[i], NULL); 
}
int main(){
	RP_init("test",THREAD_NUM);
	int iters = 10 * 1000 * 1000;
	printf("Benchmarking hooks with %d iterations:\n", iters);
	test_unhooked(iters);
	test_unhooked(iters);
	test_unhooked(iters);
	test_unhooked(iters);
	test_unhooked(iters);
	test_unhooked(iters);
	test_unhooked(iters);
	test_unhooked(iters);
	RP_close();
	cout<<"done!\n";
	return 0;
}