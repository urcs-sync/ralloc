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
echo "thread,exec_time,allocator" >> prod-con.csv
for i in {1..3}
do
	for threads in 2 4 6 10 16 20 24 32 40 48 62 72 80 84 88
	do
		rm -rf /mnt/pmem/*
		./prod-con-single.sh $threads $ALLOC
	done
done
# SEDARGS="2,\$s/$/"
# SEDARGS=${SEDARGS}","${ALLOC}"/"
# sed ${SEDARGS} -i prod-con.csv
NAME="../data/prod-con/prod-con_"${ALLOC}".csv"
cp prod-con.csv ${NAME}
