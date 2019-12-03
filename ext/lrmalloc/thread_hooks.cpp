/*
 * Copyright (C) 2019 Ricardo Leite. All rights reserved.
 * Licenced under the MIT licence. See COPYING file in the project root for details.
 */

#include <dlfcn.h>
#include <pthread.h>

#include "defines.h"
#include "lrmalloc.h"

// handle process init/exit hooks
pthread_key_t destructor_key;
bool is_initialized = false;

void initializer();
void finalizer();
void* thread_initializer(void* argptr);
void thread_finalizer(void* argptr);

LFMALLOC_ATTR(constructor)
void initializer()
{
    if (!is_initialized)
    {
        is_initialized = true;
        pthread_key_create(&destructor_key, thread_finalizer);
    }

    lf_malloc_initialize();
    lf_malloc_thread_initialize();
}

LFMALLOC_ATTR(destructor)
void finalizer()
{
    lf_malloc_thread_finalize();
    lf_malloc_finalize();
}

// handle thread init/exit hooks
typedef struct
{
    void* (*real_start)(void*);
    void* real_arg;
} thread_starter_arg;

void* thread_initializer(void* argptr)
{
    thread_starter_arg* arg = (thread_starter_arg*)argptr;
    void* (*real_start)(void*) = arg->real_start;
    void* real_arg = arg->real_arg;
    lf_malloc_thread_initialize();

    pthread_setspecific(destructor_key, (void*)1);
    return (*real_start)(real_arg);
}

void thread_finalizer(void* value)
{
    lf_malloc_thread_finalize();
}

int pthread_create(pthread_t* thread,
                   pthread_attr_t const* attr,
                   void* (start_routine)(void*),
                   void* arg)
{
    static int (*pthread_create_fn)(pthread_t*,
                                    pthread_attr_t const*,
                                    void* (void*),
                                    void*) = NULL;
    if (pthread_create_fn == NULL)
        pthread_create_fn = (int(*)(pthread_t*, pthread_attr_t const*, void* (void*), void*))dlsym(RTLD_NEXT, "pthread_create");

    // @todo: don't want to use malloc here
    // instead using a ringbuffer, which has limited storage
#define RING_BUFFER_SIZE 10000
    static std::atomic<uint32_t> ring_buffer_pos(0);
    static thread_starter_arg ring_buffer[RING_BUFFER_SIZE];
    uint32_t buffer_pos = ring_buffer_pos.fetch_add(1, std::memory_order_relaxed);

    thread_starter_arg* starter_arg = &ring_buffer[buffer_pos];
    starter_arg->real_start = start_routine;
    starter_arg->real_arg = arg;
    return pthread_create_fn(thread, attr, thread_initializer, starter_arg);
}

