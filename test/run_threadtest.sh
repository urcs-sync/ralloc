#!/bin/bash

make clean
make threadtest_test
rm -rf threadtest.csv
echo "thread, exec_time, rss" >> threadtest.csv
for i in {1..3}
do
	rm -rf /mnt/pmem/*
	./threadtest-single.sh 1
	rm -rf /mnt/pmem/*
	./threadtest-single.sh 2
	rm -rf /mnt/pmem/*
	./threadtest-single.sh 4
	rm -rf /mnt/pmem/*
	./threadtest-single.sh 8
	rm -rf /mnt/pmem/*
	./threadtest-single.sh 16
	rm -rf /mnt/pmem/*
	./threadtest-single.sh 24
	rm -rf /mnt/pmem/*
	./threadtest-single.sh 32
	rm -rf /mnt/pmem/*
	./threadtest-single.sh 40
	rm -rf /mnt/pmem/*
	./threadtest-single.sh 48
done
cp threadtest.csv ../data/threadtest.csv
