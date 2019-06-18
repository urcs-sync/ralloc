#include "thread_util.hpp"

thread_local int tid = -1;
std::atomic<uint64_t> thread_count(0);

struct thd_info{
	void *(*start_routine)(void *);
	void* arg;
	thd_info(void *(*r)(void *), void* a){start_routine = r;arg=a;}
};

void* func_wrapper (void *args){
	thd_info* thd = (thd_info*)args;
	// tid = thread_count.fetch_add(1);
	tid = 0;
	void* ret = (*thd->start_routine)(thd->arg);
	delete thd;
	return ret;
}

int pm_thread_create (pthread_t *new_thread,
					const pthread_attr_t *attr,
					void *(*start_routine)(void *), void *arg){
	thd_info* thd = new thd_info(start_routine, arg);
	return pthread_create(new_thread,attr,func_wrapper,thd);
}

int get_thread_id(){
	assert(tid!=-1);
	return tid;
}