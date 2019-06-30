# Interval Based Reclamation - Experimental Program

By Haosen Wen, Joseph Izraelevitz, Wentao Cai, H. Alan Beadle, Michael L. Scott


## Layout

### Source

* src: Source files for all experiments in the paper.
	* trackers: Implementations of memory managers mentioned in the paper.
	* rideables: Data structures for testing the memory managers. For now, all data structures are ordered maps.
* ext: External library(ies).
	* parharness: the U of Rochester synchronization group test harness	for testing parallel data structures.  Released under the Apache 2.0 license.

### Generated directories

Generated directories:

bin: Generated executables.  Organized by build configuration,
	so e.g. release/main is the release build. The most recent
	build is at the top level (bin/main).

lib : Generated .so and .a files.  Organized by build configuration,
	so e.g. release/libpolytreeX.a is the release, static library build.

obj : Generated build artifacts (.o, .d, etc.).  Also organized by build
	configuration.



## Dependencies

gcc with C++11 support
libjemalloc
libhwloc (for parharness)



## Usage

### Compilation & Execution

To compile for release:
$ make

To compile for debugging:
$ make debug

The latest executables will be in the bin directory. Use:
$ bin/main -h
for usage informations and currently available rideables and trackers.

### Use parharness

To use parharness for repeating and customizing tests, edit and run:

ext/parharness/script/testscript.py

### Draw plots

We used R for drawing plots, and a sample plotting script locates in:

data/scripts/genfigs.R

Running:

$ Rscript data/scripts/genfigs.R

Will plot out the data located in:

data/final/

by default.
