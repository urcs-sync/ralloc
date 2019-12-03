#!/bin/bash
# This is the script to insert COPYING header into every source file in ./src

for i in ./src/*.cpp ./src/*.hpp ./src/*.h 
do
  if ! grep -q Copyright $i
  then
    cat COPYING $i >$i.new && mv $i.new $i
  fi
done
