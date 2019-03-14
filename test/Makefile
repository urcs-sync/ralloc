SRC = ../src
CXX = g++

CXXFLAGS = -O0 -g -fpermissive -std=c++17 #-DDESTROY
LIBS = -lpthread -lstdc++ -latomic -fPIC 


all: msqueue_test region_manager_test base_meta_test pmmalloc_test

msqueue_test: msqueue_test.cpp
	$(CXX) -I $(SRC) -o $@ $^ $(CXXFLAGS) $(LIBS)

region_manager_test: ../obj/RegionManager.o region_manager_test.cpp
	$(CXX) -I $(SRC) -o $@ $^ $(CXXFLAGS) $(LIBS)

base_meta_test: base_meta_test.cpp libpmmalloc.a
	$(CXX) -I $(SRC) -o $@ $^ $(CXXFLAGS) $(LIBS) -L. -lpmmalloc

pmmalloc_test: pmmalloc_test.cpp libpmmalloc.a
	$(CXX) -I $(SRC) -o $@ $< $(CXXFLAGS) $(LIBS) -L. -lpmmalloc

pptr_test: pptr_test.cpp
	$(CXX) -I $(SRC) -o $@ $< $(CXXFLAGS) $(LIBS)

arraystack_test: arraystack_test.cpp libpmmalloc.a
	$(CXX) -I $(SRC) -o $@ $< $(CXXFLAGS) $(LIBS) -L. -lpmmalloc

arrayqueue_test: arrayqueue_test.cpp libpmmalloc.a
	$(CXX) -I $(SRC) -o $@ $< $(CXXFLAGS) $(LIBS) -L. -lpmmalloc

../obj/%.o: ../src/%.cpp
	$(CXX) -I $(SRC) -o $@ -c $^ $(CXXFLAGS)

libpmmalloc.a:../obj/pmmalloc.o ../obj/BaseMeta.o ../obj/RegionManager.o ../obj/thread_util.o
	ar -rcs $@ $^

clean:
	rm -f *_test
	rm -f ../obj/*
	rm -f libpmmalloc.a
	rm -f /dev/shm/*
