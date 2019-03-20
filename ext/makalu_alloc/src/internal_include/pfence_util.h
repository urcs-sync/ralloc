/*
 * We copied the methods from Romulus:
 * https://github.com/pramalhe/Romulus
 */
#ifndef PFENCE_UTIL_H
#define PFENCE_UTIL_H

#include <stdint.h>

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
         // RDTSC doesn't necessarily wait for previous instructions to complete
         // so a serializing instruction is usually used to ensure previous
         // instructions have completed. However, in our case this is a desirable
         // property since we want to overlap the latency we emulate with the
         // actual latency of the emulated instruction.
         
        stop = asm_rdtsc();
    } while (stop - start < cycles);
}

#endif