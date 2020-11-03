#!/bin/bash -x

DEPS_DIR="$(pwd)/../iresearch.deps"
export BENCHMARK_RESOURCES_ROOT="${DEPS_DIR}/benchmark_resources"
export TEST_RESOURCE_ROOT="${DEPS_DIR}/test_resources/tests/resources"
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:$(pwd)/build/bin"
ulimit -c unlimited

for i in `seq 1 1`; do
    for j in  1; do
        MAX_LINES=${j}000000

        # search
        /usr/bin/time -v ./bin/iresearch-benchmarks -m search --in ${BENCHMARK_RESOURCES_ROOT}/benchmark.tasks --dir-type mmap --index-dir iresearch.data --max-tasks 1 --repeat 2 --format 1_3simd --threads 1 --scorer=bm25 --scorer-arg='{"b":0}' --scored-terms-limit=16 --csv --topN=100 2> iresearch.stderr.${MAX_LINES}.search.log.$i 1> iresearch.stdout.${MAX_LINES}.search.log.$i &
        IRESEARCH_PID=$!
        wait $IRESEARCH_PID

        echo iresearch.stdout.${MAX_LINES}.search.log.$i
        cat iresearch.stdout.${MAX_LINES}.search.log.$i | grep 'Query' | sort
    done
done

grep "Elapsed (wall clock) time (h:mm:ss or m:ss)" iresearch.stderr.* | sort -t . -k 3 -n || true
grep "Maximum resident set size (kbytes)" iresearch.stderr.* | sort -t . -k 3 -n || true
