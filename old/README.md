This is a durably linearizable C11 implementation of Maged Michael's lock-free allocator.

It's based on the repository https://github.com/qtcwt/mmalloc.

One thing to be noticed that it uses RTM to solve ABA problem, so Intel x86-64 CPU newer than Haskell is required to run it.

#Main Author
Wentao Cai wcai6@cs.rochester.edu
Feel free to send me emails about any concerns you have.