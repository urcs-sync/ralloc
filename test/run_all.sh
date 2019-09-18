#!/bin/bash
# traditional allocator benchmarks from Hoard
./run_threadtest.sh
./run_shbench.sh
./run_larson.sh
# testing producer-consumer pattern
./run_prod-con.sh
# testing Redis TODO
# testing GC time consumption
#./run_resur.sh
