BITS = -m64
FPIC = -fPIC

CC		= gcc
CXX		= g++

CLFLAGS		= -lpthread -lm -lstdc++ -std=c++11
CFLAGS		= -D_GNU_SOURCE -D_REENTRANT #-DDEBUG

OPT = -O0 -g #-DDEBUG
CFLAGS += -Wall $(BITS) -fno-strict-aliasing $(FPIC) -mrtm

# Rules
.PHONY: all test
all: libmmalloc.so 

test: libmmalloc.so
	$(CC) $(CFLAGS) $(OPT) hookbench.c libmmalloc.so -o hookbench

.PHONY: clean
clean:
	rm -f *.o *.so hookbench

mmalloc.o: mmalloc.h mmalloc.c queue.h
	$(CC) $(CFLAGS) $(OPT) -I. -c mmalloc.c -o mmalloc.o

malloc_new.o: malloc_new.cpp mmalloc.h
	$(CXX) $(CFLAGS) $(OPT) -I. -c malloc_new.cpp -o malloc_new.o

libmmalloc.so: mmalloc.o malloc_new.o
	$(CXX) $(CLFLAGS) $(OPT) mmalloc.o malloc_new.o -o libmmalloc.so -shared

