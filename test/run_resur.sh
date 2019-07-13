#!/bin/bash

cd benchmark/Interval-Based-Reclamation; make clean;make; 
echo "total_size(MB, 50% retained), prefill_time(ms), gc_time(ms)" >> resur.csv
for i in {1..3}
do
	for MODE in {0..5}
	do
		rm -rf /dev/shm/*
		./bin/intmain/ -r2 -m$MODE -i0 -v > /tmp/resur
		while read line; do
			if [[ $line == *"Test size"* ]]; then
				test_size=$(echo $line | awk '{print $4}')
			fi
			if [[ $line == *"Prefill time"* ]]; then
				prefill_time=$(echo $line | awk '{print $4}')
				break
			fi
		done < /tmp/prod-con

		./bin/intmain/ -r2 -m$MODE -i0 -v > /tmp/resur
		while read line; do
			if [[ $line == *"Time elapsed"* ]]; then
				gc_time=$(echo $line | awk '{print $4}')
				break
			fi
		done < /tmp/prod-con
		echo "{ \"total_size\": $test_size , \"prefill_time\":  $prefill_time , \"gc_time\": $gc_time }"
		echo "$test_size, $prefill_time, $gc_time" >> resur.csv 
	done
done
cp resur.csv ../../../data/resur.csv
cd -
