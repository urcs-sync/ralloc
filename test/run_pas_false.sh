#!/bin/bash

make clean
make cache-scratch_test
rm -rf /dev/shm/*
./passive-false-single.sh 32
