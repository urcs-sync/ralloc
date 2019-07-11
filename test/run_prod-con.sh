#!/bin/bash

make clean
make prod-con_test
rm -rf prod-con.csv
echo "thread, exec_time, rss" >> prod-con.csv
for i in {1..3}
do
	rm -rf /dev/shm/*
	./prod-con-single.sh 2
	rm -rf /dev/shm/*
	./prod-con-single.sh 4
	rm -rf /dev/shm/*
	./prod-con-single.sh 8
	rm -rf /dev/shm/*
	./prod-con-single.sh 12
	rm -rf /dev/shm/*
	./prod-con-single.sh 16
	rm -rf /dev/shm/*
	./prod-con-single.sh 24
	rm -rf /dev/shm/*
	./prod-con-single.sh 32
	rm -rf /dev/shm/*
	./prod-con-single.sh 40
	rm -rf /dev/shm/*
	./prod-con-single.sh 48
	rm -rf /dev/shm/*
	./prod-con-single.sh 64
	rm -rf /dev/shm/*
	./prod-con-single.sh 72
	rm -rf /dev/shm/*
	./prod-con-single.sh 80 
done
cp prod-con.csv ../data/prod-con.csv