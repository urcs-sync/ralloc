# Ralloc - Recoverable Lock-free Allocator

Ralloc is a persistent lock-free allocator designed for nonvolatile memory.
It is introduced in *Understanding and Optimizing Persistent
Memory Allocation* by Wentao Cai, Haosen Wen, H. Alan Beadle, Chris Kjellqvist, 
Mohammad Hedayati, and Michael L. Scott. The full paper is to appear in ISMM' 20. 
You can also find the brief announcement version of the paper in PPoPP' 20.

## Warning

Current implementation doesn't support filter functions in pure C code!

## Layout

* src: Ralloc's Source files.
* ext: External library.
    * makalu_alloc: the Makalu source code from [Atlas
      repo](https://github.com/HewlettPackard/Atlas/tree/makalu) on GitHub.
    * lrmalloc: the LRMalloc source code from [lrmalloc
      repo](https://github.com/ricleite/lrmalloc) on GitHub, with some minor bug
      fix and retweak. 
      WARNING: Due to the small counter used in LRMalloc to avoid ABA
      problem, it has been encountered that a hazard counter overflow will
      happen and cause segfaults when #thread is big (e.g., >72). Please be
      aware of this if you want to run benchmarks with LRMalloc.
* test: testing code and Makefile.
    * ./: running scripts and Makefile; executables of benchmarks; libralloc.a
    * benchmark: macros and benchmarks source code.
* data: 
    * genfigs.R: plotting script.
    * Others: Generated csv files by testing scripts.
* obj: Generated build artifacts (.o) organized by test/Makefile

## Dependencies

#### Necessary
gcc with C++11 support

libjemalloc

#### Optional
PMDK if you want to run benchmarks with it

## Usage
All following commands assume that you are in the root directory of this
repo.

By default, Ralloc creates files in NVM mounted at `/mnt/pmem/`. If you want
to instead emulate using DRAM, i.e., create files in `/dev/shm/`, please define 
macro `SHM_SIMULATING` while building. More details can be found in the 
*macros* Section below.

### Use Ralloc in your projects

To use ralloc in other projects :

`$ cd test`

`$ make libralloc.a`

And then 
1. add `#include "ralloc.hpp"` to files that use Ralloc's functions.
2. append `-I<path_to_ralloc>/src` to your compile command.
3. link libralloc.a to your project by appending
`-L<path_to_ralloc>/test -lralloc.a` to your link command.

### Benchmarks

To compile libralloc.a and all benchmarks :

`$ cd test`

`$ make ALLOC=<r|mak|je|lr|pmdk>`

By default ALLOC is r.

To compile specific target :

`$ cd test`

`$ make <libralloc.a|threadtest_test|sh6bench_test|larson_test|prod-con_test> ALLOC=<r|mak|je|lr|pmdk>`

### Execution

To run all benchmarks with all allocators, do :

`$ cd test`

`$ ./run_all.sh`

The results will be written in csv files stored in ./data.

To run a specific benchmark with a particular allocator, do :

`$ cd test`

`$ ./run_<larson|prod-con|shbench|threadtest>.sh <r|mak|je|lr|pmdk>`

The results will be written in csv files stored in ./data/$0/$0_$1.csv.
($0 can be larson, prod-con, shbench, or threadtest; $1 can be r, mak, je, lr,
or pmdk.)

### Draw plots
We used R for drawing plots, and a sample plotting script locates in:

`data/genfigs.R`

Running:

`$ cd data`

`$ Rscript ./genfigs.R`

Will plot out the data located in:

`data/`

## License

This project is licensed under the MIT license. You may find a copy
of the license in the
[LICENSE](https://github.com/qtcwt/ralloc/blob/master/LICENSE) file 
included in the Ralloc source distribution.

## Macros

### DESTROY

This macro enables the option to destroy all mapping files during the exit. This
might be useful for benchmarking.

### SHM_SIMULATING

This macro switches Ralloc to compatible mode for machines with no real
persistent memory. In this mode, ramdisk located in `/dev/shm` will be used.

When this macro is not defined, allocations go to `/mnt/pmem` by default. 
If your mounting point of the persistent memory is different, please replace
`/mnt/pmem/` with yours in `src/pm_config.hpp`.

## Test with different allocator

This is controlled by the following macros, but we recommend the user may to select 
different allocators by passing corresponding `ALLOC` to `make`. `test/Makefile` will 
handle `ALLOC` and define the corresponding macro to enable the target allocator, as
we mentioned in the previous *Benchmarks* section. 

### RALLOC

Run with Ralloc, a lock-free persistent allocator by University of Rochester.

### MAKALU

Run with Makalu, a lock-based persistent allocator by HP Lab.

### PMDK

Run with libpmemobj from PMDK, a persistent memory programming toolkit by Intel.

### otherwise

Directly call `malloc` and `free`. jemalloc is used by default
