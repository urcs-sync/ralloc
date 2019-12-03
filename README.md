# Ralloc - Recoverable Lock-free Allocator

By Wentao Cai, Haosen Wen, H. Alan Beadle, Mohammad Hedayati and Michael L.
Scott

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
    * Interval-Based-Reclamation: source code of interval based reclamation
      framework, used for testing Ralloc's recovery.
    * redis: clone from [redis repo](https://github.com/antirez/redis) in
      GitHub. This is a TODO for benchmarking Ralloc.
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
NOTE: all following commands assume that you are in the root directory of this
repo.
### Compilation

To compile libralloc.a and all benchmarks :
$ cd test
$ make ALLOC=<r|mak|je|lr|pmdk>
By default ALLOC is r.

To compile specific target :
$ cd test
$ make <libralloc.a|threadtest_test|sh6bench_test|larson_test|prod-con_test> ALLOC=<r|mak|je|lr|pmdk>

### Execution

To run all benchmarks with all allocators, do :
$ cd test
$ ./run_all.sh
The results will be written in csv files stored in ./data.

To run a specific benchmark with a particular allocator, do :
$ cd test
$ ./run_<larson|prod-con|shbench|threadtest>.sh <r|mak|je|lr|pmdk>
The results will be written in csv files stored in ./data/$0/$0_$1.csv.
($0 can be larson, prod-con, shbench, or threadtest; $1 can be r, mak, je, lr,
or pmdk.)

### Draw plots
We used R for drawing plots, and a sample plotting script locates in:

data/genfigs.R

Running:

$ cd data
$ Rscript ./genfigs.R

Will plot out the data located in:

./data/

by default.

## License

This project is licensed under Apache License, Version 2.0. You may find a copy
of the license in the [COPYING][1] file included in the Ralloc source
distribution.
[1]: https://github.com/qtcwt/ralloc/COPYING