#!/bin/bash
# traditional allocator benchmarks from Hoard
for alloc in  "lr" "r" "mak" "pmdk" # "je" 
do
	./run_larson.sh $alloc
	./run_shbench.sh $alloc
	./run_threadtest.sh $alloc
	# testing producer-consumer pattern
	./run_prod-con.sh $alloc
	# testing Redis TODO
	# testing GC time consumption
	#./run_resur.sh
done
