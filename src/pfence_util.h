/*
 * Copyright (C) 2019 University of Rochester. All rights reserved.
 * Licenced under the MIT licence. See LICENSE file in the project root for
 * details. 
 */

#ifndef PFENCE_UTIL_H
#define PFENCE_UTIL_H

#include <stdint.h>

/*
 * This file contains 3 versions of flush and fence macros:
 * 1. PWB_IS_CLFLUSH
 *    This uses clflush as flush, and noop as fence since (sequential) clflush
 *    doesn't need explicit fence to order.
 * 2. PWB_IS_CLWB
 *    This uses clwb as flush and sfence as fence.
 * 3. PWB_IS_PCM
 *    This only emulates the latency of persistent memory and has no effect on
 *    writeback behavior.
 * 
 * To be compatible on machines with no clwb (which usually aren't equipped by
 * real persistent memory), we use macro SHM_SIMULATING to switch between
 * clflush and clwb.
 */

// Uncomment to enable durable linearizability
#define DUR_LIN

#ifdef SHM_SIMULATING
  #define PWB_IS_CLFLUSH
#else
  #define PWB_IS_CLWB
#endif

#ifdef DUR_LIN
  #ifdef PWB_IS_NOOP
    #define FLUSH(addr)
    #define FLUSHFENCE 
  #elif defined(PWB_IS_CLFLUSH)
    #define FLUSH(addr) asm volatile ("clflush (%0)" :: "r"(addr))
    #define FLUSHFENCE 
  #elif defined(PWB_IS_CLWB)
    #define FLUSH(addr) asm volatile ("clwb (%0)" :: "r"(addr))
    #define FLUSHFENCE asm volatile ("sfence" ::: "memory")
  #elif defined(PWB_IS_PCM)
    #define FLUSH(addr) emulate_latency_ns(340)
    #define FLUSHFENCE emulate_latency_ns(500)
  #else
    #error "Please define what PWB is."
  #endif /* PWB_IS_? */
#else /* !DUR_LIN */
    #define FLUSH(addr) 
    #define FLUSHFENCE 
#endif

/*
 * We copied the methods from Romulus:
 * https://github.com/pramalhe/Romulus
 */

static inline unsigned long long asm_rdtsc(void)
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

// Change this depending on the clock cycle of your cpu. For Cervino it's 2100, for my laptop it's 2712.
#define EMULATED_CPUFREQ  2300

#define NS2CYCLE(__ns) ((__ns) * EMULATED_CPUFREQ / 1000)

static inline void emulate_latency_ns(int ns) {
    uint64_t stop;
    uint64_t start = asm_rdtsc();
    uint64_t cycles = NS2CYCLE(ns);
    do {
        /* RDTSC doesn't necessarily wait for previous instructions to complete
         * so a serializing instruction is usually used to ensure previous
         * instructions have completed. However, in our case this is a desirable
         * property since we want to overlap the latency we emulate with the
         * actual latency of the emulated instruction.
         */
        stop = asm_rdtsc();
    } while (stop - start < cycles);
}

#endif
