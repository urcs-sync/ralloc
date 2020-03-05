#!/bin/bash

if [[ $# -lt 1 ]]; then
  echo "usage: shbench-single.sh  <num threads>"
  echo ""
  echo "wraps a single run of shbench with rss sampling"
  echo ""
  echo "example:"
  echo "  ./shbench-single.sh 1"
  exit 1
fi

if [[ $# -ne 2 ]]; then
  ALLOC="r"
else
  ALLOC=$2
fi

BINARY=./sh6bench_test
if [ "$ALLOC" == "je" ]; then
  BINARY="numactl --membind=2 "${BINARY}
fi

THREADS=$1

PARAMS=/tmp/shbench_params
echo "100000" > $PARAMS
echo "64" >> $PARAMS
echo "400" >> $PARAMS
echo "$THREADS" >> $PARAMS

rm -f /tmp/shbench
$BINARY < $PARAMS  > /tmp/shbench

while read line; do
  if [[ $line == *"rdtsc time"* ]]; then
    exec_time=$(echo $line | awk '{print $3}')
    break
  fi
done < /tmp/shbench

echo "{ \"threads\": $THREADS , \"time\":  $exec_time , \"allocator\": $ALLOC }"
echo "$THREADS,$exec_time,$ALLOC" >> shbench.csv
