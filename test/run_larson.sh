#!/bin/bash

make clean
make larson_test
rm -rf larson.csv
echo "thread, ops, rss" >> larson.csv
for i in {1..3}
do
	rm -rf /mnt/pmem/*
	./larson-single.sh 1
	rm -rf /mnt/pmem/*
	./larson-single.sh 2
	rm -rf /mnt/pmem/*
	./larson-single.sh 4
	rm -rf /mnt/pmem/*
	./larson-single.sh 8
	rm -rf /mnt/pmem/*
	./larson-single.sh 16
	rm -rf /mnt/pmem/*
	./larson-single.sh 24
	rm -rf /mnt/pmem/*
	./larson-single.sh 32
	rm -rf /mnt/pmem/*
	./larson-single.sh 40
	rm -rf /mnt/pmem/*
	./larson-single.sh 48
done
cp larson.csv ../data/larson.csv
