#
# Copyright (C) 2019 Ricardo Leite. All rights reserved.
# Licenced under the MIT licence. See COPYING file in the project root for details.
#

CCX=g++
DFLAGS=-ggdb -g -fno-omit-frame-pointer
CXXFLAGS=-shared -fPIC -std=gnu++14 -O3 -Wall $(DFLAGS) \
	-fno-builtin-malloc -fno-builtin-free -fno-builtin-realloc \
	-fno-builtin-calloc -fno-builtin-cfree -fno-builtin-memalign \
	-fno-builtin-posix_memalign -fno-builtin-valloc -fno-builtin-pvalloc \
	-fno-builtin -fsized-deallocation

LDFLAGS=-ldl -pthread

OBJFILES=lrmalloc.o size_classes.o pages.o pagemap.o tcache.o thread_hooks.o

default: lrmalloc.so lrmalloc.a

%.o : %.cpp
	$(CCX) $(CXXFLAGS) -c -o $@ $< $(LDFLAGS)

lrmalloc.so: $(OBJFILES)
	$(CCX) $(CXXFLAGS) -o lrmalloc.so $(OBJFILES) $(LDFLAGS)

lrmalloc.a: $(OBJFILES)
	ar rcs lrmalloc.a $(OBJFILES)

clean:
	rm -f *.so *.o *.a
