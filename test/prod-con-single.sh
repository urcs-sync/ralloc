#!/bin/bash

if [[ $# -lt 1 ]]; then
  echo "usage: prod-con-single.sh <even num threads>"
  echo ""
  echo "wraps a single run of prod-con with rss sampling"
  echo ""
  echo "example:"
  echo "  ./prod-con-single.sh 2"
  exit 1
fi

if [[ $# -ne 2 ]]; then
  ALLOC="r"
else
  ALLOC=$2
fi

BINARY=./prod-con_test
if [ "$ALLOC" == "je" ]; then
  BINARY="numactl --membind=2 "${BINARY}
fi

THREADS=$1

rm -f /tmp/prod-con
$BINARY $THREADS 10000000 64 > /tmp/prod-con 

while read line; do
  if [[ $line == *"Time elapsed"* ]]; then
    exec_time=$(echo $line | awk '{print $4}')
    break
  fi
done < /tmp/prod-con

echo "{ \"threads\": $THREADS , \"time\":  $exec_time , \"allocator\": $ALLOC}"
echo "$THREADS,$exec_time,$ALLOC" >> prod-con.csv
