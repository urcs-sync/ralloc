# macros

## DESTROY

This macro enables the option to destroy all mapping files during the exit. This
might be useful for benchmarking.

## SHM_SIMULATING

This macro switches Ralloc to compatible mode for machines with no real
persistent memory. In this mode, ramdisk located in `/dev/shm` will be used.

When this macro is not defined, allocations go to `/mnt/pmem`. If your 
mounting point of the persistent memory is different, then simply replace
`/mnt/pmem/` by yours in `src/pm_config.hpp`.

## Test with different allocator

This is controlled by following macros, but the user may want to do this by
passing corresponding `ALLOC` to make (which is written in test/Makefile).

### RALLOC

Run with Ralloc, a lock-free persistent allocator by University of Rochester.

### MAKALU

Run with Makalu, a lock-based persistent allocator by HP Lab.

### PMDK

Run with libpmemobj from PMDK, a persistent memory programming toolkit by Intel.

### otherwise
directly call malloc and free. jemalloc is used by default
