#!/bin/bash

make clean
make threadtest_test
rm -rf /dev/shm/*
./threadtest-single.sh 4 
