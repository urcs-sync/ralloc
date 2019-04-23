#!/bin/bash

make clean
make sh6bench_test
rm -rf shbench.csv
echo "thread, exec_time, rss" >> shbench.csv
for i in {1..3}
do
	rm -rf /dev/shm/*
	./shbench-single.sh 1
	rm -rf /dev/shm/*
	./shbench-single.sh 2
	rm -rf /dev/shm/*
	./shbench-single.sh 4
	rm -rf /dev/shm/*
	./shbench-single.sh 8
	rm -rf /dev/shm/*
	./shbench-single.sh 16
	rm -rf /dev/shm/*
	./shbench-single.sh 32
	rm -rf /dev/shm/*
	./shbench-single.sh 48
	rm -rf /dev/shm/*
	./shbench-single.sh 64
	rm -rf /dev/shm/*
	./shbench-single.sh 72
	rm -rf /dev/shm/*
	./shbench-single.sh 80 
done
cp shbench.csv ../data/shbench.csv
