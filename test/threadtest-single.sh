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
$BINARY $THREADS 10000 100000 0 64 > /tmp/threadtest &
pid=$!

renice -n 19 -p $$ > /dev/null

n=0
rss=0
while true ; do
  rss_sample=$(ps --no-headers -o "rss" $pid)
  (( n += 1 ))
  (( rss += rss_sample ))
  sleep 0.05
  while read line; do
    if [[ $line == *"Time elapsed"* ]]; then
      exec_time=$(echo $line | awk '{print $4}')
      break 2
    fi
  done < /tmp/threadtest
done
(( rss = rss / n ))

exec_time=$(echo $line | awk '{print $4}')
echo "{ \"threads\": $THREADS , \"time\":  $exec_time , \"rss\": $rss }"
