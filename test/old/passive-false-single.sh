#!/bin/bash

if [[ $# -ne 1 ]]; then
  echo "usage: passive-false-single.sh <num threads>"
  echo ""
  echo "wraps a single run of passive-false aka cache-scratch"
  echo ""
  echo "example:"
  echo "  ./passive-false-single.sh 1 "
  exit 1
fi

BINARY=./cache-scratch_test
THREADS=$1
rm -f /tmp/cache-scratch
$BINARY $THREADS 1000 8 1000000 2>&1 > /tmp/cache-scratch &
pid=$!

renice -n 19 -p $$ > /dev/null

while true ; do
  while read line; do
    if [[ $line == *"elapsed"* ]]; then
      t=$(echo $line | awk '{print $4}')
      break 2
    fi
  done < /tmp/cache-scratch
done

echo "{ \"threads\": $THREADS , \"time\":  $t  }"

echo "$THREADS, $t" >> pas.csv
