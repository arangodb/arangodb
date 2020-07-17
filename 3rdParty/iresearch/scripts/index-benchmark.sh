#!/bin/bash -x

DEPS_DIR="$(pwd)/../iresearch.deps"
export BENCHMARK_RESOURCES_ROOT="${DEPS_DIR}/benchmark_resources"
export TEST_RESOURCE_ROOT="${DEPS_DIR}/test_resources/tests/resources"
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:$(pwd)/bin"
ulimit -c unlimited

for i in `seq 1 1`; do
    for j in 25 ; do
        MAX_LINES=${j}000000

        rm -r iresearch.data || {
            ls -la bin && cat iresearch.stderr.${MAX_LINES}.index.log.$i
        }

        # index
        /usr/bin/time -v ./bin/iresearch-benchmarks -m put --in ${BENCHMARK_RESOURCES_ROOT}/benchmark.data --index-dir iresearch.data --max-lines=${MAX_LINES} --format 1_3simd --commit-period=10000 --batch-size=50000 --consolidation-threads 1 --threads=8 2> iresearch.stderr.${MAX_LINES}.index.log.$i 1> /dev/null &
        IRESEARCH_PID=$!
        wait $IRESEARCH_PID

        echo iresearch.stdout.${MAX_LINES}.search.log.$i
        cat iresearch.stdout.${MAX_LINES}.search.log.$i | grep 'Query execution' | sort
    done
done

grep "Elapsed (wall clock) time (h:mm:ss or m:ss)" iresearch.stderr.* | sort -t . -k 3 -n || true
grep "Maximum resident set size (kbytes)" iresearch.stderr.* | sort -t . -k 3 -n || true
