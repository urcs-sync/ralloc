
## Introduction
----
lrmalloc is an efficient, lock-free malloc(3) implementation.

It is derived from [Michael's lock-free allocator](https://dl.acm.org/citation.cfm?doid=996841.996848), improved with modern memory allocator features such as thread caches and allocator/user memory segregation.

lrmalloc's philosophy is to provide fast synchronization-free allocations as much sa possible through the use of thread caches, and only use lock-free operations to fill and empty thread caches.

## Usage
----
To compile, just download this repository and run 
```console
make
```

If successfully compiled, you can link lrmalloc with your application at compile time with
```console
-llrmalloc
```
or you can dynamically link it with your application by using LD_PRELOAD (if your application was not statically linked with another memory allocator).
```console
LD_PRELOAD=lrmalloc.so ./your_application
```
## Copyright

License: MIT

Read file [COPYING](COPYING).

