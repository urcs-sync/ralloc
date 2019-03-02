SRC = ../src
CXX = g++

CXXFLAGS = -O0 -g -fpermissive -std=c++17
LIBS = -lpthread -lstdc++ -fPIC


all: msqueue_test region_manager_test base_meta_test pmmalloc_test

msqueue_test: msqueue_test.cpp
	$(CXX) -I $(SRC) -o $@ $^ $(CXXFLAGS) $(LIBS)

region_manager_test: ../obj/RegionManager.o region_manager_test.cpp
	$(CXX) -I $(SRC) -o $@ $^ $(CXXFLAGS) $(LIBS)
	rm -f /dev/shm/test*

base_meta_test: base_meta_test.cpp libpmmalloc.a
	$(CXX) -I $(SRC) -o $@ $^ $(CXXFLAGS) $(LIBS) -L. -lpmmalloc
	rm -f /dev/shm/test*

pmmalloc_test: pmmalloc_test.cpp libpmmalloc.a
	$(CXX) -I $(SRC) -o $@ $< $(CXXFLAGS) $(LIBS) -L. -lpmmalloc
	rm -f /dev/shm/test*

../obj/%.o: ../src/%.cpp
	$(CXX) -I $(SRC) -o $@ -c $^ $(CXXFLAGS)

libpmmalloc.a:../obj/pmmalloc.o ../obj/BaseMeta.o ../obj/RegionManager.o ../obj/thread_util.o
	ar -rcs $@ $^

clean:
	rm -f *_test
	rm -f ../obj/*
	rm -f libpmmalloc.a
