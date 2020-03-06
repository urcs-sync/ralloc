#!/bin/bash

if [[ $# -lt 1 ]]; then
  echo "usage: larson-single.sh <num threads>"
  echo ""
  echo "wraps a single run of larson with rss sampling"
  echo ""
  echo "example:"
  echo "  ./larson-single.sh 1"
  exit 1
fi

if [[ $# -ne 2 ]]; then
  ALLOC="r"
else
  ALLOC=$2
fi

BINARY=./larson_test
if [ "$ALLOC" == "je" ]; then
  BINARY="numactl --membind=2 "${BINARY}
fi

THREADS=$1

rm -f /tmp/larson
$BINARY 30 64 400 1000 10000 123 $THREADS > /tmp/larson

while read line; do
  if [[ $line == *"Throughput"* ]]; then
    ops=$(echo $line | awk '{print $3}')
    break
  fi
done < /tmp/larson

echo "{ \"threads\": $THREADS , \"ops\":  $ops , \"allocator\": $ALLOC}"
echo "$THREADS,$ops,$ALLOC" >> larson.csv
