SRC = ../src
CXX = g++

WARNING_FLAGS:=-ftrapv -Wreturn-type -W -Wall \
-Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-parameter

FLAGS = -O3 -g -fpermissive $(WARNING_FLAGS) -fno-omit-frame-pointer #-DDESTROY -DMEM_CONSUME_TEST
PMMALLOC_FLAGS = $(FLAGS) -DPMMALLOC -L.
MAKALU_FLAGS = $(FLAGS) -I../ext/makalu_alloc/include -DMAKALU -L../ext/makalu_alloc/lib -lmakalu 
PMDK_FLAGS = $(FLAGS) -DPMDK -lpmemobj 

R_CXXFLAGS = $(PMMALLOC_FLAGS) -ljemalloc -L. -lrpmalloc 
MAK_CXXFLAGS = $(MAKALU_FLAGS) -ljemalloc 
JE_CXXFLAGS = $(FLAGS) -ljemalloc
LR_CXXFLAGS = $(FLAGS) -L../../lrmalloc -l:lrmalloc.a -ldl# for built-in malloc
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
	CXXFLAGS = $(LR_CXXFLAGS)
endif

ifeq ($(ALLOC),pmdk)
	CXXFLAGS = $(PMDK_CXXFLAGS)
endif

LIBS = -pthread -lstdc++ -latomic -fPIC 

all: benchmark_pm

trivial_test: trivial_test.cpp
	$(CXX) -I $(SRC) -o $@ $^ $(CXXFLAGS) $(LIBS)

msqueue_test: msqueue_test.cpp
	$(CXX) -I $(SRC) -o $@ $^ $(CXXFLAGS) $(LIBS)

region_manager_test: ../obj/RegionManager.o region_manager_test.cpp
	$(CXX) -I $(SRC) -o $@ $^ $(CXXFLAGS) $(LIBS)

base_meta_test: base_meta_test.cpp librpmalloc.a
	$(CXX) -I $(SRC) -o $@ $^ $(CXXFLAGS) $(LIBS) -L. -lrpmalloc

rpmalloc_test: rpmalloc_test.cpp librpmalloc.a
	$(CXX) -I $(SRC) -o $@ $< $(CXXFLAGS) $(LIBS) -L. -lrpmalloc

pptr_test: pptr_test.cpp librpmalloc.a
	$(CXX) -I $(SRC) -o $@ $< $(CXXFLAGS) $(LIBS) -L. -lrpmalloc

arraystack_test: arraystack_test.cpp librpmalloc.a
	$(CXX) -I $(SRC) -o $@ $< $(CXXFLAGS) $(LIBS) -L. -lrpmalloc

arrayqueue_test: arrayqueue_test.cpp librpmalloc.a
	$(CXX) -I $(SRC) -o $@ $< $(CXXFLAGS) $(LIBS) -L. -lrpmalloc

../obj/%.o: ../src/%.cpp
	$(CXX) -I $(SRC) -o $@ -c $^ $(CXXFLAGS)

benchmark_pm: threadtest_test sh6bench_test larson_test prod-con_test #cache-scratch_test cache-thrash_test

threadtest_test: ./benchmark/threadtest.cpp librpmalloc.a
	$(CXX) -I $(SRC) -I ./benchmark -o $@ $< $(CXXFLAGS) $(LIBS) 

sh6bench_test: ./benchmark/sh6bench.cpp librpmalloc.a
	$(CXX) -I $(SRC) -I ./benchmark -o $@ $< $(CXXFLAGS) $(LIBS) 

larson_test: ./benchmark/larson.cpp librpmalloc.a
	$(CXX) -I $(SRC) -I ./benchmark -o $@ $< $(CXXFLAGS) $(LIBS) 

# # Active false sharing
# cache-thrash_test: ./benchmark/cache-thrash.cpp librpmalloc.a
# 	$(CXX) -I $(SRC) -I ./benchmark -o $@ $< $(CXXFLAGS) $(LIBS) -L. -lrpmalloc

# # Passive false sharing
# cache-scratch_test: ./benchmark/cache-scratch.cpp librpmalloc.a
# 	$(CXX) -I $(SRC) -I ./benchmark -o $@ $< $(CXXFLAGS) $(LIBS) -L. -lrpmalloc

prod-con_test: ./benchmark/prod-con.cpp librpmalloc.a
	$(CXX) -I $(SRC) -I ./benchmark -o $@ $< $(CXXFLAGS) $(LIBS) 

librpmalloc.a:../obj/SizeClass.o ../obj/RegionManager.o ../obj/TCache.o ../obj/BaseMeta.o ../obj/rpmalloc.o
	ar -rcs $@ $^

clean:
	rm -f *_test
	rm -rf ../obj/*
	rm -f librpmalloc.a
	rm -rf /mnt/pmem/*
