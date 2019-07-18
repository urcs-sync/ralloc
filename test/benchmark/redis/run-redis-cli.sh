#!/bin/bash
action=$1
bin=./src/redis-cli
time='timeout 1m'

if [[ $action == '-h' ]]
then
        $bin -h
elif [[ $action == '--small' ]]
then
	$time $bin --lru-test 10000
elif [[ $action == '--med' ]]
then
	$time $bin --lru-test 100000
elif [[ $action == '--large' ]]
then
	$time $bin --lru-test 1000000
else
	exit
fi


