#!/bin/bash

if [[ $# -lt 1 ]]; then
  echo "usage: threadtest-single.sh <num threads>"
  echo ""
  echo "wraps a single run of threadtest with rss sampling"
  echo ""
  echo "example:"
  echo "  ./threadtest-single.sh 1 "
  exit 1
fi

if [[ $# -ne 2 ]]; then
  ALLOC="r"
else
  ALLOC=$2
fi

BINARY=./threadtest_test
if [ "$ALLOC" == "je" ]; then
  BINARY="numactl --membind=2 "${BINARY}
fi

THREADS=$1

rm /tmp/threadtest
$BINARY $THREADS 10000 100000 0 8 > /tmp/threadtest

while read line; do
  if [[ $line == *"Time elapsed"* ]]; then
    exec_time=$(echo $line | awk '{print $4}')
    break
  fi
done < /tmp/threadtest

echo "{ \"threads\": $THREADS , \"time\":  $exec_time , \"allocator\": $ALLOC}"
echo "$THREADS,$exec_time,$ALLOC" >> threadtest.csv
