############################################################################
# Download benchmark resources
############################################################################
DEPS_DIR="$(pwd)/../iresearch.deps"
BENCHMARK_RESOURCES_DIR="benchmark_resources"
BENCHMARK_DATA_URL="http://home.apache.org/~mikemccand/enwiki-20120502-lines-1k.txt.lzma"
BENCHMARK_INDEX_JAR_URL="https://www.dropbox.com/s/asdqtd3jbfv8q46/indexer-j8.jar?dl=0" # compiled with Java 1.8
BENCHMARK_SEARCH_JAR_URL="https://www.dropbox.com/s/qf9ot583oh34ekl/searcher-j8.jar?dl=0" # compiled with Java 1.8
#BENCHMARK_INDEX_JAR_URL="https://www.dropbox.com/s/mqxm9gov031xmva/indexer.jar?dl=0" # compiled with Java 1.9
#BENCHMARK_SEARCH_JAR_URL="https://www.dropbox.com/s/yp1gzwrl0oufbma/searcher.jar?dl=0" # compiled with Java 1.9
#BENCHMARK_TASKS_URL="https://raw.githubusercontent.com/mikemccand/luceneutil/master/tasks/wikimedium.1M.nostopwords.tasks"
BENCHMARK_TASKS_URL="https://raw.githubusercontent.com/iresearch-toolkit/iresearch-resources/master/benchmarks/iresearch-benchmark.tasks"

#rm -rf "${DEPS_DIR}/${BENCHMARK_RESOURCES_DIR}"
[ -f "${DEPS_DIR}/${BENCHMARK_RESOURCES_DIR}/benchmark.data" ] || rm -rf "${DEPS_DIR}/${BENCHMARK_RESOURCES_DIR}"
[ -d "${DEPS_DIR}/${BENCHMARK_RESOURCES_DIR}" ] || {
    mkdir -p "${DEPS_DIR}/${BENCHMARK_RESOURCES_DIR}"
    cd "${DEPS_DIR}/${BENCHMARK_RESOURCES_DIR}"
    wget --quiet -O - "${BENCHMARK_DATA_URL}" | lzma -d > benchmark.data
    wget --quiet -O lucene_index.jar "${BENCHMARK_INDEX_JAR_URL}"
    wget --quiet -O lucene_search.jar "${BENCHMARK_SEARCH_JAR_URL}"
    wget --quiet -O - "${BENCHMARK_TASKS_URL}" | grep -E 'HighTerm:|MedTerm:|LowTerm:|HighPhrase:|MedPhrase:|LowPhrase:|AndHighHigh:|AndHighMed:|AndHighLow:|OrHighHigh:|OrHighMed:|OrHighLow:|Prefix3:|Wildcard:|Fuzzy1:|Fuzzy2:' > benchmark.tasks
}
