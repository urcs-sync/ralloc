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
make prod-con_test ${ARGS}
rm -rf prod-con.csv
echo "thread, exec_time, allocator" >> prod-con.csv
for i in {1..3}
do
	for threads in 2 4 6 8 10 12 16 20 24 32
	do
		rm -rf /mnt/pmem/*
		./prod-con-single.sh $threads
	done
done
SEDARGS="2,\$s/$/"
SEDARGS=${SEDARGS}","${ALLOC}"/"
sed ${SEDARGS} -i prod-con.csv
NAME="../data/prod-con/prod-con_"${ALLOC}".csv"
cp prod-con.csv ${NAME}
