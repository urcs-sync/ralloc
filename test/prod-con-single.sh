#!/bin/bash

if [[ $# -ne 1 ]]; then
  echo "usage: prod-con-single.sh <even num threads>"
  echo ""
  echo "wraps a single run of prod-con with rss sampling"
  echo ""
  echo "example:"
  echo "  ./prod-con-single.sh 2"
  exit 1
fi

BINARY=./prod-con_test
THREADS=$1

rm -f /tmp/prod-con
$BINARY $THREADS 10000000 64 > /tmp/prod-con &
pid=$!

renice -n 19 -p $$ > /dev/null

n=0
rss=0
while true ; do
  rss_sample=$(ps --no-headers -o "rss" $pid)
  #echo "$rss_sample"
  (( n += 1 ))
  (( rss += rss_sample ))
  sleep 0.1
  while read line; do
    if [[ $line == *"Time elapsed"* ]]; then
      exec_time=$(echo $line | awk '{print $4}')
      break 2
    fi
  done < /tmp/prod-con
done
(( rss = rss / n ))

exec_time=$(echo $line | awk '{print $4}')
echo "{ \"threads\": $THREADS , \"time\":  $exec_time , \"rss\": $rss }"

echo "$THREADS, $exec_time, $rss" >> prod-con.csv