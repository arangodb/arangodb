#!/bin/bash -x

DEPS_DIR="$(pwd)/../iresearch.deps"
export BENCHMARK_RESOURCES_ROOT="${DEPS_DIR}/benchmark_resources"
export TEST_RESOURCE_ROOT="${DEPS_DIR}/test_resources/tests/resources"
PATH=${PATH}:${BISON_ROOT}/bin
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:$(pwd)/build/bin"
for file in $(find / -name java -type f -executable 2>/dev/null); do PATH="${PATH}:$(dirname "${file}")"; done # ensure java is in the PATH
rm -rf build
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -g "Unix Makefiles" .. || exit 1
make -j 16 iresearch-benchmarks || exit 1
cd bin
ulimit -c unlimited

for i in `seq 1 5`; do
    for j in 1 5 10 15 20 25; do
        MAX_LINES=${j}000000

        # index
        /usr/bin/time -v ./iresearch-benchmarks -m put --in ${BENCHMARK_RESOURCES_ROOT}/benchmark.data --index-dir ../iresearch.data --max-lines=${MAX_LINES} --commit-period=1000 --batch-size=10000 --threads=8 2> iresearch.stderr.${MAX_LINES}.index.log.$i 1> /dev/null &
        IRESEARCH_PID=$!
        /usr/bin/time -v java -jar ${BENCHMARK_RESOURCES_ROOT}/lucene_index.jar -dirImpl MMapDirectory -analyzer StandardAnalyzer -lineDocsFile ${BENCHMARK_RESOURCES_ROOT}/benchmark.data -maxConcurrentMerges 1 -ramBufferMB -1 -postingsFormat Lucene50 -waitForMerges -mergePolicy LogDocMergePolicy -idFieldPostingsFormat Memory -grouping -waitForCommit -indexPath ../lucene.data -docCountLimit ${MAX_LINES} -maxBufferedDocs 10000 -threadCount 8 2> lucene.stderr.${MAX_LINES}.index.log.$i 1> /dev/null &
        LUCENE_PID=$!
        wait $IRESEARCH_PID $LUCENE_PID 

        # search
        /usr/bin/time -v ./iresearch-benchmarks -m search --in ${BENCHMARK_RESOURCES_ROOT}/benchmark.tasks --dir-type mmap --index-dir ../iresearch.data --max-tasks 1 --repeat 20 --threads 8 --scorer=bm25 --scorer-arg='{"b":0}' --scored-terms-limit=16 --csv --topN=100 2> iresearch.stderr.${MAX_LINES}.search.log.$i 1> iresearch.stdout.${MAX_LINES}.search.log.$i &
        IRESEARCH_PID=$!
        /usr/bin/time -v java -server -Xms2g -Xmx40g -XX:-TieredCompilation -XX:+HeapDumpOnOutOfMemoryError -Xbatch -jar ${BENCHMARK_RESOURCES_ROOT}/lucene_search.jar -dirImpl MMapDirectory -indexPath ../lucene.data -analyzer StandardAnalyzer -taskSource ${BENCHMARK_RESOURCES_ROOT}/benchmark.tasks -searchThreadCount 8 -taskRepeatCount 20 -field body -tasksPerCat 1 -staticSeed -6486775 -seed -6959386 -similarity BM25Similarity -commit multi -hiliteImpl FastVectorHighlighter -log lucene.stdlog.${MAX_LINES}.search.log.$i -csv -topN 100 -pk 2> lucene.stderr.${MAX_LINES}.search.log.$i 1> lucene.stdout.${MAX_LINES}.search.log.$i &
        LUCENE_PID=$!
        wait $IRESEARCH_PID $LUCENE_PID 

        for k in lucene.stdlog.${MAX_LINES}.search.log.$i; do
            mv $k $k.raw
            cat $k.raw | grep ',' \
            | awk -F ',' '{do {++count[$1];sum[$1]+=$5;} while(getline);for (category in sum) {printf("Lucene Query execution (%s) time calls:%i, time: %.2f us, avg call: %.2f us\n", category, count[category], sum[category], sum[category]/count[category]);}}' > $k
        done

        echo iresearch.stdout.${MAX_LINES}.search.log.$i
        cat iresearch.stdout.${MAX_LINES}.search.log.$i | grep 'Query execution' | sort
        echo lucene.stdlog.${MAX_LINES}.search.log.$i
        cat lucene.stdlog.${MAX_LINES}.search.log.$i | grep 'Query execution' | sort
#tar czf iresearch.data.$i.$j.tgz ../iresearch.data
        du -s ../iresearch.data | awk '{print $1}' >  iresearch.${MAX_LINES}.indexSize.log.$i
        rm -r ../iresearch.data || {
            ls -la bin && cat iresearch.stderr.${MAX_LINES}.index.log.$i
        }
        du -s ../lucene.data | awk '{print $1}' >  lucene.${MAX_LINES}.indexSize.log.$i
        rm -r ../lucene.data || {
            ls -la bin && cat lucene.stderr.${MAX_LINES}.index.log.$i
        }
    done
done

grep "Elapsed (wall clock) time (h:mm:ss or m:ss)" iresearch.stderr.* | sort -t . -k 3 -n || true
grep "Elapsed (wall clock) time (h:mm:ss or m:ss)" lucene.stderr.* | sort -t . -k 3 -n || true
grep "Maximum resident set size (kbytes)" iresearch.stderr.* | sort -t . -k 3 -n || true
grep "Maximum resident set size (kbytes)" lucene.stderr.* | sort -t . -k 3 -n || true
