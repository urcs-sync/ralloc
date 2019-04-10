#!/bin/bash

make clean
make cache-scratsh_test
rm -rf /dev/shm/*
./passive-false-single.sh 4