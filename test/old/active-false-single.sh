#!/bin/bash

if [[ $# -ne 1 ]]; then
  echo "usage: active-false-single.sh <num threads>"
  echo ""
  echo "wraps a single run of active-false aka cache-thrash"
  echo ""
  echo "example:"
  echo "  ./active-false-single.sh 1 "
  exit 1
fi

BINARY=./cache-thrash_test
THREADS=$1
rm -f /tmp/cache-thrash
$BINARY $THREADS 1000 8 1000000 2>&1 > /tmp/cache-thrash &
pid=$!

renice -n 19 -p $$ > /dev/null

while true ; do
  while read line; do
    if [[ $line == *"elapsed"* ]]; then
      t=$(echo $line | awk '{print $4}')
      break 2
    fi
  done < /tmp/cache-thrash
done

echo "{ \"threads\": $THREADS , \"time\":  $t  }"
echo "$THREADS, $t" >> act.csv