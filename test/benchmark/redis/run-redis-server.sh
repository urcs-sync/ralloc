#!/bin/bash
if [[ $1 == '--trace' ]]
then
	trace='y'
else
	trace='n'
fi

export PMEM_TRACE_ENABLE=$trace	# y or n
export PMEM_NO_MOVNT=1
export PMEM_MMAP_HINT=0x0000100000000000
export PMEM_IS_PMEM_FORCE=1

bin=./src/redis-server

if [[ $1 == '-h' ]]
then
	$bin -h
else
	$bin ./redis.conf &
fi


