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
make larson_test ${ARGS}
rm -rf larson.csv
echo "thread, ops, rss, allocator" >> larson.csv
for i in {1..3}
do
	for threads in 1 2 4 8 12 16 24 32 40 48
	do
		rm -rf /mnt/pmem/*
		./larson-single.sh $threads
	done
done
SEDARGS="2,\$s/$/"
SEDARGS=${SEDARGS}${ALLOC}"/"
echo $SEDARGS
sed ${SEDARGS} -i larson.csv
NAME="../data/larson/larson_"${ALLOC}".csv"
cp larson.csv ${NAME}
