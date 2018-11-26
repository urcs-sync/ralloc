BITS = -m64
FPIC = -fPIC

CC		= gcc
CXX		= g++

CLFLAGS		= -lpthread -lm -lstdc++ -std=c++11
CFLAGS		= -D_GNU_SOURCE -D_REENTRANT #-DDEBUG

OPT = -O0 -g #-DDEBUG
CFLAGS += -Wall $(BITS) -fno-strict-aliasing $(FPIC)

# Rules
.PHONY: all test
all: libpmmalloc.a 

test: libpmmalloc.a
	$(CC) $(CFLAGS) $(OPT) hookbench.c libpmmalloc.a -lpthread -o hookbench

.PHONY: clean
clean:
	rm -f *.o *.so hookbench

pmmalloc.o: pmmalloc.h pmmalloc.c queue.h
	$(CC) $(CFLAGS) $(OPT) -I. -c pmmalloc.c -o pmmalloc.o

malloc_new.o: malloc_new.cpp pmmalloc.h
	$(CXX) $(CFLAGS) $(OPT) -I. -c malloc_new.cpp -o malloc_new.o

libpmmalloc.a: pmmalloc.o malloc_new.o
	ar rcs libpmmalloc.a pmmalloc.o 

