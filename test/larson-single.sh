#!/bin/bash

if [[ $# -ne 1 ]]; then
  echo "usage: larson-single.sh <num threads>"
  echo ""
  echo "wraps a single run of larson with rss sampling"
  echo ""
  echo "example:"
  echo "  ./larson-single.sh 1"
  exit 1
fi

BINARY=./larson_test
THREADS=$1

rm -f /tmp/larson
$BINARY 10 56 64 1000 10000 123 $THREADS > /tmp/larson &
pid=$!

renice -n 19 -p $$ > /dev/null

while true ; do
  sleep 0.1
  while read line; do
    if [[ $line == *"Throughput"* ]]; then
      ops=$(echo $line | awk '{print $3}')
      break 2
    fi
  done < /tmp/larson
done

echo "{ \"threads\": $THREADS , \"ops\":  $ops}"
echo "$THREADS, $ops" >> larson.csv
