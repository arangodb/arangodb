list(APPEND CMAKE_MODULE_PATH "${frozen_SOURCE_DIR}/cmake")
include(sed)

find_package(benchmark REQUIRED)

add_executable(frozen.benchmark "")

target_link_libraries(frozen.benchmark PUBLIC
  frozen::frozen
  benchmark::benchmark)

option(frozen.benchmark.str_search
  "Build Benchmark Boyer-Moore string search (requires C++17 compiler)" OFF)

target_compile_options(frozen.benchmark PUBLIC
  $<$<BOOL:${frozen.benchmark.str_search}>:cxx_std_17>)

sed(${CMAKE_CURRENT_LIST_DIR}/bench_int_set.cpp
    ${frozen_BINARY_DIR}/benchmarks/bench_int_unordered_set.cpp
    "set" "unordered_set" "Set" "UnorderedSet")

sed(${CMAKE_CURRENT_LIST_DIR}/bench_str_set.cpp
    ${frozen_BINARY_DIR}/benchmarks/bench_str_unordered_set.cpp
    "set" "unordered_set" "Set" "UnorderedSet")

target_sources(frozen.benchmark PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/bench_main.cpp
  ${CMAKE_CURRENT_LIST_DIR}/bench_int_set.cpp
  ${CMAKE_CURRENT_LIST_DIR}/bench_str_set.cpp
  ${frozen_BINARY_DIR}/benchmarks/bench_int_unordered_set.cpp
  ${frozen_BINARY_DIR}/benchmarks/bench_str_unordered_set.cpp
  $<$<BOOL:${frozen.benchmark.str_search}>:
    ${CMAKE_CURRENT_LIST_DIR}/bench_str_search.cpp>)

add_custom_target(benchmark frozen.benchmark)
