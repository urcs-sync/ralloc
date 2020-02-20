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
make threadtest_test ${ARGS}
rm -rf threadtest.csv
echo "thread,exec_time,allocator" >> threadtest.csv
for i in {1..3}
do
	for threads in 1 2 4 6 10 16 20 24 32 40 48 62 72 80 84 88
	do
		rm -rf /mnt/pmem/*
		./threadtest-single.sh $threads $ALLOC
	done
done
# SEDARGS="2,\$s/$/"
# SEDARGS=${SEDARGS}","${ALLOC}"/"
# echo $SEDARGS
# sed ${SEDARGS} -i threadtest.csv
NAME="../data/threadtest/threadtest_"${ALLOC}".csv"
cp threadtest.csv ${NAME}
