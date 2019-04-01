#!/bin/bash

make clean
make larson_test
rm -rf /dev/shm/*
./larson-single.sh 1 
rm -rf /dev/shm/*
./larson-single.sh 2 
rm -rf /dev/shm/*
./larson-single.sh 4 
rm -rf /dev/shm/*
./larson-single.sh 8 
rm -rf /dev/shm/*
./larson-single.sh 16 
rm -rf /dev/shm/*
./larson-single.sh 32
rm -rf /dev/shm/*
./larson-single.sh 48
rm -rf /dev/shm/*
./larson-single.sh 64
rm -rf /dev/shm/*
./larson-single.sh 72 
