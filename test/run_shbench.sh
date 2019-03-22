#!/bin/bash

make clean
make sh6bench_test
rm -rf /dev/shm/*
./shbench-single.sh 4 
