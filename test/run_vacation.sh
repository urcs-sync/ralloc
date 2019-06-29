#!/bin/bash

cd ../../../PersistentWordSTM/mnemosyne-gcc;make clean;make all;cd -;
cd ../../../PersistentWordSTM/mnemosyne-gcc/usermode
rm -rf vacation.csv
echo "thread, time, rss" >> vacation.csv
for i in {1..3}
do
	./test-vacation.sh
done
cp vacation.csv ../../../plfmalloc/rpmalloc/data/vacation.csv
cd -