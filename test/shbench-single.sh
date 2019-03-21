#!/bin/bash

if [[ $# -ne 1 ]]; then
  echo "usage: shbench-single.sh  <num threads>"
  echo ""
  echo "wraps a single run of shbench with rss sampling"
  echo ""
  echo "example:"
  echo "  ./shbench-single.sh 1"
  exit 1
fi

BINARY=./sh6bench_test
THREADS=$1

PARAMS=/tmp/shbench_params
echo "1000000" > $PARAMS
echo "1" >> $PARAMS
echo "8" >> $PARAMS
echo "$THREADS" >> $PARAMS

rm -f /tmp/shbench
$BINARY < $PARAMS  > /tmp/shbench &
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
    if [[ $line == *"rdtsc time"* ]]; then
      exec_time=$(echo $line | awk '{print $3}')
      break 2
    fi
  done < /tmp/shbench
done
(( rss = rss / n ))

#exec_time=$(echo $line | awk '{print $4}')
echo "{ \"threads\": $THREADS , \"time\":  $exec_time , \"rss\": $rss }"
