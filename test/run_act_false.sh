#!/bin/bash

make clean
make cache-thrash_test
rm -rf /dev/shm/*
./active-false-single.sh 4
