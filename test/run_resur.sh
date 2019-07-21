#!/bin/bash

make clean;make threadtest_test
cd benchmark/Interval-Based-Reclamation; make clean;make; 
echo "reachable_blocks, prefill_time(ms), gc_time(ms)" >> resur.csv
for i in {1..3}
do
	for MODE in {0..5}
	do
		rm -rf /dev/shm/*
		./bin/intmain -r3 -m$MODE -i0 -v > /tmp/resur
		while read line; do
			if [[ $line == *"Prefill time"* ]]; then
				prefill_time=$(echo $line | awk '{print $4}')
				break
			fi
		done < /tmp/resur

		./bin/intmain -r3 -m$MODE -i0 -v > /tmp/resur
		while read line; do
			if [[ $line == *"Time elapsed"* ]]; then
				gc_time=$(echo $line | awk '{print $4}')
			fi
			if [[ $line == *"Reachable blocks"* ]]; then
				reachable=$(echo $line | awk '{print $4}')
			fi
		done < /tmp/resur
		echo "{ \"reachable blocks\": $reachable , \"prefill_time\":  $prefill_time , \"gc_time\": $gc_time }"
		echo "$reachable, $prefill_time, $gc_time" >> resur.csv 
	done
done
cp resur.csv ../../../data/resur.csv
cd -
