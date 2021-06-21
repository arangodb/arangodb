set (IResearchBenchmark_TARGET_NAME "iresearch-microbench" CACHE INTERNAL "")

find_package(benchmark REQUIRED)

add_executable(${IResearchBenchmark_TARGET_NAME}
  ./top_term_collector_benchmark.cpp
  ./microbench_main.cpp
)

set_ipo(${IResearchBenchmark_TARGET_NAME})
add_sanitizers(${IResearchBenchmark_TARGET_NAME})

add_dependencies(${IResearchBenchmark_TARGET_NAME}
  ${IResearch_TARGET_NAME}-analyzer-delimiter-static
  ${IResearch_TARGET_NAME}-analyzer-ngram-static
  ${IResearch_TARGET_NAME}-analyzer-text-static
  ${IResearch_TARGET_NAME}-analyzer-norm-static
  ${IResearch_TARGET_NAME}-analyzer-stem-static
  ${IResearch_TARGET_NAME}-analyzer-stopwords-static
  ${IResearch_TARGET_NAME}-format-1_0-static
  ${IResearch_TARGET_NAME}-scorer-tfidf-static
  ${IResearch_TARGET_NAME}-scorer-bm25-static
)

target_include_directories(${IResearchBenchmark_TARGET_NAME}
  PRIVATE ${PROJECT_BINARY_DIR}/core
)

set_target_properties(${IResearchBenchmark_TARGET_NAME}
  PROPERTIES
  OUTPUT_NAME iresearch-microbench
  COMPILE_DEFINITIONS "$<$<CONFIG:Coverage>:IRESEARCH_DEBUG>;$<$<CONFIG:Debug>:IRESEARCH_DEBUG>;FST_NO_DYNAMIC_LINKING"
)

target_link_libraries(${IResearchBenchmark_TARGET_NAME}
  ${IResearch_TARGET_NAME}-static-allinone
  ${PTHREAD_LIBRARY}
  benchmark::benchmark
)

include_directories(${IResearchBenchmark_TARGET_NAME}
  ${EXTERNAL_INCLUDE_DIRS}
  ${IResearchTests_INCLUDE_DIR}
  ${IReSearch_generated_INCLUDE_DIR}
  ${IResearch_INCLUDE_DIR}
  ${ICU_INCLUDE_DIR}
  ${GTEST_INCLUDE_DIR}
  ${Boost_INCLUDE_DIRS}
  ${FROZEN_INCLUDE_DIR}
  $<TARGET_PROPERTY:absl-static,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:${IResearch_TARGET_NAME}-cmdline,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:${IResearch_TARGET_NAME}-rapidjson,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:${IResearch_TARGET_NAME}-ofst,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:${IResearch_TARGET_NAME}-utfcpp,INTERFACE_INCLUDE_DIRECTORIES>
)
