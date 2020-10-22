##
# Copyright (C) 2019 University of Rochester. All rights reserved.
# Licenced under the MIT licence. See LICENSE file in the project root for
# details. 
##

SRC = ../src
OBJ = ../obj
CXX = g++

WARNING_FLAGS:=-ftrapv -Wreturn-type -W -Wall \
-Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-parameter

FLAGS = -O3 -g -fpermissive $(WARNING_FLAGS) -fno-omit-frame-pointer -fPIC #-DSHM_SIMULATING #-DDESTROY -DMEM_CONSUME_TEST
RALLOC_FLAGS = $(FLAGS) -DRALLOC -L.
MAKALU_FLAGS = $(FLAGS) -I../ext/makalu_alloc/include -DMAKALU -L../ext/makalu_alloc/lib -lmakalu 
PMDK_FLAGS = $(FLAGS) -DPMDK -lpmemobj 

R_CXXFLAGS = $(RALLOC_FLAGS) -ljemalloc -L. -lralloc 
MAK_CXXFLAGS = $(MAKALU_FLAGS) -ljemalloc 
JE_CXXFLAGS = $(FLAGS) -ljemalloc
LR_CXXFLAGS = $(FLAGS) -L../ext/lrmalloc -l:lrmalloc.a -ldl# for built-in malloc
PMDK_CXXFLAGS = $(PMDK_FLAGS) -ljemalloc

# Ralloc by default
CXXFLAGS = $(R_CXXFLAGS)
ifeq ($(ALLOC),r)
	CXXFLAGS = $(R_CXXFLAGS)
endif

ifeq ($(ALLOC),mak)
	CXXFLAGS = $(MAK_CXXFLAGS)
endif

ifeq ($(ALLOC),je)
	CXXFLAGS = $(JE_CXXFLAGS)
endif

ifeq ($(ALLOC),lr)
	# Ralloc without flush and fence is effectively LRMalloc, with optimization
	CXXFLAGS = $(R_CXXFLAGS) -DPWB_IS_NOOP
endif

ifeq ($(ALLOC),pmdk)
	CXXFLAGS = $(PMDK_CXXFLAGS)
endif

LIBS = -pthread -lstdc++ -latomic 

all: benchmark_pm

# trivial_test: trivial_test.cpp
# 	$(CXX) -I $(SRC) -o $@ $^ $(CXXFLAGS) $(LIBS)
# 
# msqueue_test: msqueue_test.cpp
# 	$(CXX) -I $(SRC) -o $@ $^ $(CXXFLAGS) $(LIBS)
# 
# region_manager_test: ../obj/RegionManager.o region_manager_test.cpp
# 	$(CXX) -I $(SRC) -o $@ $^ $(CXXFLAGS) $(LIBS)
# 
# base_meta_test: base_meta_test.cpp libralloc.a
# 	$(CXX) -I $(SRC) -o $@ $^ $(CXXFLAGS) $(LIBS) -L. -lralloc
# 
# ralloc_test: ralloc_test.cpp libralloc.a
# 	$(CXX) -I $(SRC) -o $@ $< $(CXXFLAGS) $(LIBS) -L. -lralloc
# 
# pptr_test: pptr_test.cpp libralloc.a
# 	$(CXX) -I $(SRC) -o $@ $< $(CXXFLAGS) $(LIBS) -L. -lralloc
# 
# arraystack_test: arraystack_test.cpp libralloc.a
# 	$(CXX) -I $(SRC) -o $@ $< $(CXXFLAGS) $(LIBS) -L. -lralloc
# 
# arrayqueue_test: arrayqueue_test.cpp libralloc.a
# 	$(CXX) -I $(SRC) -o $@ $< $(CXXFLAGS) $(LIBS) -L. -lralloc

SOURCES := $(wildcard $(SRC)/*.cpp)
OBJECTS := $(patsubst $(SRC)/%.cpp, $(OBJ)/%.o, $(SOURCES))

$(OBJ)/%.o: $(SRC)/%.cpp
	$(CXX) -I $(SRC) -o $@ -c $^ $(CXXFLAGS)

benchmark_pm: threadtest_test sh6bench_test larson_test prod-con_test #cache-scratch_test cache-thrash_test

threadtest_test: ./benchmark/threadtest.cpp libralloc.a
	$(CXX) -I $(SRC) -I ./benchmark -o $@ $^ $(CXXFLAGS) $(LIBS) 

sh6bench_test: ./benchmark/sh6bench.cpp libralloc.a
	$(CXX) -I $(SRC) -I ./benchmark -o $@ $^ $(CXXFLAGS) $(LIBS) 

larson_test: ./benchmark/larson.cpp libralloc.a
	$(CXX) -I $(SRC) -I ./benchmark -o $@ $^ $(CXXFLAGS) $(LIBS) 

# # Active false sharing
# cache-thrash_test: ./benchmark/cache-thrash.cpp libralloc.a
# 	$(CXX) -I $(SRC) -I ./benchmark -o $@ $^ $(CXXFLAGS) $(LIBS) -L. -lralloc

# # Passive false sharing
# cache-scratch_test: ./benchmark/cache-scratch.cpp libralloc.a
# 	$(CXX) -I $(SRC) -I ./benchmark -o $@ $^ $(CXXFLAGS) $(LIBS) -L. -lralloc

prod-con_test: ./benchmark/prod-con.cpp libralloc.a
	$(CXX) -I $(SRC) -I ./benchmark -o $@ $^ $(CXXFLAGS) $(LIBS) 

libralloc.a: $(OBJECTS)
	ar -rcs $@ $^

clean:
	rm -f *_test
	rm -rf ../obj/*
	rm -f libralloc.a
	rm -rf /mnt/pmem/*
