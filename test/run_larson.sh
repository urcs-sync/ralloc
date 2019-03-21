#!/bin/bash

make clean
make larson_test
rm -rf /dev/shm/*
./larson-single.sh 4 
