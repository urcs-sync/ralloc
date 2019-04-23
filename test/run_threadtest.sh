#!/bin/bash

make clean
make threadtest_test
rm -rf threadtest.csv
echo "thread, exec_time, rss" >> threadtest.csv
for i in {1..3}
do
	rm -rf /dev/shm/*
	./threadtest-single.sh 1
	rm -rf /dev/shm/*
	./threadtest-single.sh 2
	rm -rf /dev/shm/*
	./threadtest-single.sh 4
	rm -rf /dev/shm/*
	./threadtest-single.sh 8
	rm -rf /dev/shm/*
	./threadtest-single.sh 16
	rm -rf /dev/shm/*
	./threadtest-single.sh 32
	rm -rf /dev/shm/*
	./threadtest-single.sh 48
	rm -rf /dev/shm/*
	./threadtest-single.sh 64
	rm -rf /dev/shm/*
	./threadtest-single.sh 72
	rm -rf /dev/shm/*
	./threadtest-single.sh 80 
done
cp threadtest.csv ../data/threadtest.csv