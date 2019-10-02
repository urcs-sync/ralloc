#!/bin/bash

make clean
make sh6bench_test
rm -rf shbench.csv
echo "thread, exec_time, rss" >> shbench.csv
for i in {1..3}
do
	rm -rf /mnt/pmem/*
	./shbench-single.sh 1
	rm -rf /mnt/pmem/*
	./shbench-single.sh 2
	rm -rf /mnt/pmem/*
	./shbench-single.sh 4
	rm -rf /mnt/pmem/*
	./shbench-single.sh 8
	rm -rf /mnt/pmem/*
	./shbench-single.sh 16
	rm -rf /mnt/pmem/*
	./shbench-single.sh 24
	rm -rf /mnt/pmem/*
	./shbench-single.sh 32
	rm -rf /mnt/pmem/*
	./shbench-single.sh 40
	rm -rf /mnt/pmem/*
	./shbench-single.sh 48
done
cp shbench.csv ../data/shbench.csv
