#!/bin/bash

make clean
make cache-thrash_test
rm -rf act.csv
echo "thread, time" >> act.csv
for i in {1..3}
do
	rm -rf /dev/shm/*
	./active-false-single.sh 1
	rm -rf /dev/shm/*
	./active-false-single.sh 2
	rm -rf /dev/shm/*
	./active-false-single.sh 4
	rm -rf /dev/shm/*
	./active-false-single.sh 8
	rm -rf /dev/shm/*
	./active-false-single.sh 16
	rm -rf /dev/shm/*
	./active-false-single.sh 32
	rm -rf /dev/shm/*
	./active-false-single.sh 48
	rm -rf /dev/shm/*
	./active-false-single.sh 64
	rm -rf /dev/shm/*
	./active-false-single.sh 72
	rm -rf /dev/shm/*
	./active-false-single.sh 80 
done
cp act.csv ../data/act.csv