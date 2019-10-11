#!/bin/bash
if [[ $# -ne 1 ]]; then
    ALLOC="r"
else
    ALLOC=$1
fi
ARGS="ALLOC="
ARGS=${ARGS}${ALLOC}
echo $ARGS

make clean
make sh6bench_test ${ARGS}
rm -rf shbench.csv
echo "thread, exec_time, rss, allocator" >> shbench.csv
for i in {1..3}
do
	for threads in 1 2 4 8 12 16 24 32 40 48
	do
		rm -rf /mnt/pmem/*
		./shbench-single.sh $threads
	done
done
SEDARGS="2,\$s/$/"
SEDARGS=${SEDARGS}${ALLOC}"/"
echo $SEDARGS
sed ${SEDARGS} -i shbench.csv
NAME="../data/shbench/shbench_"${ALLOC}".csv"
cp shbench.csv ${NAME}
