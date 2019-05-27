#macros
##DEBUG
This macro enables verbose output for debugging
##DESTROY
This macro enables the option to destroy all mapping files during the exit
##GC
This macro enables garbage collection to fix inconsistency when you restart from a dirty metadata (i.e. the allocator exited unexpectedly last time). Otherwise, durable linearizability will be achieved by online flush and fence.
##Test with different allocator
###PMMALLOC
Run with rpmalloc
###MAKALU
Run with makalu
###otherwise
directly call malloc and free. jemalloc is used by default
