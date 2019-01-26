SRC = ../src
CXX = g++

CFLAGS = -O0 -g -fpermissive -std=c++17
LIBS = -lpthread -lstdc++


all: msqueue_test region_manager_test base_meta_test

msqueue_test: msqueue_test.cpp
	$(CXX) -I $(SRC) -o $@ $^ $(CFLAGS) $(LIBS)

region_manager_test: ../obj/RegionManager.o region_manager_test.cpp
	$(CXX) -I $(SRC) -o $@ $^ $(CFLAGS) $(LIBS)
	rm -f /dev/shm/test

base_meta_test: ../obj/RegionManager.o base_meta_test.cpp
	$(CXX) -I $(SRC) -o $@ $^ $(CFLAGS) $(LIBS)
	rm -f /dev/shm/test

../obj/RegionManager.o: ../src/RegionManager.cpp
	$(CXX) -I $(SRC) -o $@ -c $^ $(CFLAGS)

clean:
	rm -f *_test
	rm -f ../obj/*

