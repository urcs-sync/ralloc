#ifndef THREAD_UTIL_H
#define THREAD_UTIL_H

#include <pthread.h>
#include <atomic>
#include <assert.h>
extern thread_local int tid;
extern std::atomic<uint64_t> thread_count;

extern void* func_wrapper (void *args);

extern int pm_thread_create (pthread_t *new_thread,
					const pthread_attr_t *attr,
					void *(*start_routine)(void *), void *arg);

extern int get_thread_id();

#endif