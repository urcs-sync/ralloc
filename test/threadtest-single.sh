#!/bin/bash

if [[ $# -ne 1 ]]; then
  echo "usage: threadtest-single.sh <num threads>"
  echo ""
  echo "wraps a single run of threadtest with rss sampling"
  echo ""
  echo "example:"
  echo "  ./threadtest-single.sh 1 "
  exit 1
fi

BINARY=./threadtest_test
THREADS=$1

rm /tmp/threadtest
$BINARY $THREADS 10000 100000 0 8 > /tmp/threadtest &
pid=$!

renice -n 19 -p $$ > /dev/null
while true ; do
  sleep 0.1
  while read line; do
    if [[ $line == *"Time elapsed"* ]]; then
      exec_time=$(echo $line | awk '{print $4}')
      break 2
    fi
  done < /tmp/threadtest
done

echo "{ \"threads\": $THREADS , \"time\":  $exec_time }"

echo "$THREADS, $exec_time" >> threadtest.csv
