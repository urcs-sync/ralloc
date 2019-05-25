SRC = ../src
CXX = g++

WARNING_FLAGS:=-ftrapv -Wreturn-type -W -Wall \
-Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-parameter

FLAGS = -O3 -g -fpermissive -std=c++17 $(WARNING_FLAGS) -DDESTROY
PMMALLOC_FLAGS = $(FLAGS) -DPMMALLOC -L.
PMMALLOC_GC_FLAGS = $(FLAGS) -DGC -DPMMALLOC -L.
MAKALU_FLAGS = $(FLAGS) -I../ext/makalu_alloc/include -DMAKALU -L../ext/makalu_alloc/lib -lmakalu -L~/.local/lib/
CXXFLAGS = $(PMMALLOC_GC_FLAGS)
# CXXFLAGS = $(MAKALU_FLAGS)
# CXXFLAGS = $(FLAGS) #-L../../lrmalloc -l:lrmalloc.a -ldl# for built-in malloc
LIBS = -pthread -lstdc++ -latomic -fPIC -ljemalloc 


all: benchmark_pm

trivial_test: trivial_test.cpp
	$(CXX) -I $(SRC) -o $@ $^ $(CXXFLAGS) $(LIBS)

msqueue_test: msqueue_test.cpp
	$(CXX) -I $(SRC) -o $@ $^ $(CXXFLAGS) $(LIBS)

region_manager_test: ../obj/RegionManager.o region_manager_test.cpp
	$(CXX) -I $(SRC) -o $@ $^ $(CXXFLAGS) $(LIBS)

base_meta_test: base_meta_test.cpp libpmmalloc.a
	$(CXX) -I $(SRC) -o $@ $^ $(CXXFLAGS) $(LIBS) -L. -lpmmalloc

pmmalloc_test: pmmalloc_test.cpp libpmmalloc.a
	$(CXX) -I $(SRC) -o $@ $< $(CXXFLAGS) $(LIBS) -L. -lpmmalloc

pptr_test: pptr_test.cpp libpmmalloc.a
	$(CXX) -I $(SRC) -o $@ $< $(CXXFLAGS) $(LIBS) -L. -lpmmalloc

arraystack_test: arraystack_test.cpp libpmmalloc.a
	$(CXX) -I $(SRC) -o $@ $< $(CXXFLAGS) $(LIBS) -L. -lpmmalloc

arrayqueue_test: arrayqueue_test.cpp libpmmalloc.a
	$(CXX) -I $(SRC) -o $@ $< $(CXXFLAGS) $(LIBS) -L. -lpmmalloc

../obj/%.o: ../src/%.cpp
	$(CXX) -I $(SRC) -o $@ -c $^ $(CXXFLAGS)

benchmark_pm: threadtest_test sh6bench_test larson_test cache-scratch_test cache-thrash_test

threadtest_test: ./benchmark/threadtest.cpp libpmmalloc.a
	$(CXX) -I $(SRC) -I ./benchmark -o $@ $< $(CXXFLAGS) $(LIBS) -L. -lpmmalloc

sh6bench_test: ./benchmark/sh6bench.cpp libpmmalloc.a
	$(CXX) -I $(SRC) -I ./benchmark -o $@ $< $(CXXFLAGS) $(LIBS) -L. -lpmmalloc

larson_test: ./benchmark/larson.cpp libpmmalloc.a
	$(CXX) -I $(SRC) -I ./benchmark -o $@ $< $(CXXFLAGS) $(LIBS) -L. -lpmmalloc

# Active false sharing
cache-thrash_test: ./benchmark/cache-thrash.cpp libpmmalloc.a
	$(CXX) -I $(SRC) -I ./benchmark -o $@ $< $(CXXFLAGS) $(LIBS) -L. -lpmmalloc

# Passive false sharing
cache-scratch_test: ./benchmark/cache-scratch.cpp libpmmalloc.a
	$(CXX) -I $(SRC) -I ./benchmark -o $@ $< $(CXXFLAGS) $(LIBS) -L. -lpmmalloc

libpmmalloc.a:../obj/SizeClass.o ../obj/RegionManager.o ../obj/PageMap.o ../obj/TCache.o ../obj/BaseMeta.o ../obj/pmmalloc.o
	ar -rcs $@ $^

clean:
	rm -f *_test
	rm -f ../obj/*
	rm -f libpmmalloc.a
	rm -f /dev/shm/*
