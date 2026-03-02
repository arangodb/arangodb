SOURCE_GROUP("analysis" ./analysis/*)
SOURCE_GROUP("store" ./store/*)
SOURCE_GROUP("index" ./index/*)
SOURCE_GROUP("formats" ./formats/*)
SOURCE_GROUP("search" ./search/*)
SOURCE_GROUP("utils" ./utils/*)

set(IReSearch_generated_INCLUDE_DIR
  ${CMAKE_CURRENT_BINARY_DIR}
  )

set(IResearchTests_INCLUDE_DIR
  "${PROJECT_SOURCE_DIR}/tests"
  CACHE INTERNAL
  ""
  )

set(IReSearch_tests_sources
  ./analysis/analyzer_test.cpp
  ./analysis/delimited_token_stream_tests.cpp
  ./analysis/collation_token_stream_test.cpp
  ./analysis/classification_stream_test.cpp
  ./analysis/nearest_neighbors_stream_test.cpp
  ./analysis/ngram_token_stream_test.cpp
  ./analysis/minhash_token_stream_test.cpp
  ./analysis/multi_delimited_token_stream_tests.cpp
  ./analysis/pipeline_stream_tests.cpp
  ./analysis/segmentation_stream_tests.cpp
  ./analysis/text_token_normalizing_stream_tests.cpp
  ./analysis/text_token_stemming_stream_tests.cpp
  ./analysis/token_stopwords_stream_tests.cpp
  ./analysis/token_attributes_test.cpp
  ./analysis/token_stream_tests.cpp
  ./formats/columnstore2_test.cpp
  ./formats/sparse_bitmap_test.cpp
  ./formats/formats_tests.cpp
  ./formats/formats_test_case_base.cpp
  ./formats/skip_list_test.cpp
  ./formats/address_table_tests.cpp
  ./store/directory_test_case.cpp
  ./store/caching_directory_test.cpp
  ./store/directory_cleaner_tests.cpp
  ./store/memory_index_output_tests.cpp
  ./store/store_utils_tests.cpp
  ./index/doc_generator.cpp
  ./index/assert_format.cpp
  ./index/index_meta_tests.cpp
  ./index/index_profile_tests.cpp
  ./index/index_tests.cpp
  ./index/index_levenshtein_tests.cpp
  ./index/index_column_tests.cpp
  ./index/norm_test.cpp
  ./index/sorted_index_tests.cpp
  ./index/index_death_tests.cpp
  ./index/field_meta_test.cpp
  ./index/merge_writer_tests.cpp
  ./index/postings_tests.cpp
  ./index/buffered_column_test.cpp
  ./index/segment_writer_tests.cpp
  ./index/consolidation_policy_tests.cpp
  ./search/empty_filter_tests.cpp
  ./search/nested_filter_test.cpp
  ./search/granular_range_filter_tests.cpp
  ./search/wildcard_filter_test.cpp
  ./search/levenshtein_filter_test.cpp
  ./search/index_reader_test.cpp
  ./search/scorers_tests.cpp
  ./search/bitset_doc_iterator_test.cpp
  ./search/sort_tests.cpp
  ./search/tfidf_test.cpp
  ./search/wand_test.cpp
  ./search/bm25_test.cpp
  ./search/cost_attribute_test.cpp
  ./search/boost_attribute_test.cpp
  ./search/filter_test_case_base.cpp
  ./search/boolean_filter_tests.cpp
  ./search/all_filter_tests.cpp
  ./search/term_filter_tests.cpp
  ./search/terms_filter_test.cpp
  ./search/prefix_filter_test.cpp
  ./search/range_filter_test.cpp
  ./search/phrase_filter_tests.cpp
  ./search/column_existence_filter_test.cpp
  ./search/same_position_filter_tests.cpp
  ./search/ngram_similarity_filter_tests.cpp
  ./search/top_terms_collector_test.cpp
  ./search/all_iterator_tests.cpp
  ./search/proxy_filter_test.cpp
  ./utils/async_utils_tests.cpp
  ./utils/automaton_test.cpp
  ./utils/bitvector_tests.cpp
  ./utils/encryption_test.cpp
  ./utils/container_utils_tests.cpp
  ./utils/compression_test.cpp
  ./utils/crc_test.cpp
  ./utils/file_utils_tests.cpp
  ./utils/object_pool_tests.cpp
  ./utils/numeric_utils_test.cpp
  ./utils/attributes_tests.cpp
  ./utils/directory_utils_tests.cpp
  ./utils/bit_packing_tests.cpp
  ./utils/bit_utils_tests.cpp
  ./utils/block_pool_test.cpp
  ./utils/levenshtein_utils_test.cpp
  ./utils/wildcard_utils_test.cpp
  ./utils/ref_counter_tests.cpp
  ./utils/minhash_utils_test.cpp
  ./utils/string_tests.cpp
  ./utils/simd_utils_test.cpp
  ./utils/bitset_tests.cpp
  ./utils/math_utils_test.cpp
  ./utils/misc_test.cpp
  ./utils/std_test.cpp
  ./utils/thread_pool_test.cpp
  ./utils/type_utils_tests.cpp
  ./utils/utf8_utils_test.cpp
  ./utils/utf8_path_tests.cpp
  ./utils/fst_builder_test.cpp
  ./utils/fst_utils_test.cpp
  ./utils/fst_string_weight_test.cpp
  ./utils/ngram_match_utils_tests.cpp
  ./memory/IResearchMemoryManager_tests.cpp
  ./tests_param.cpp
  ./tests_main.cpp
  )

set(IReSearch_tests_headers
  ./index/assert_format.hpp
  ./index/doc_generator.hpp
  ./index/index_tests.hpp
  ./search/filter_test_case_base.hpp
  ./formats/formats_test_case_base.hpp
  ./tests_config.hpp
  ${IReSearch_generated_INCLUDE_DIR}/tests_config.hpp # auto generated
  )

if (MSVC)
  # disable "checked iterators" feature
  add_definitions(-D_SCL_SECURE_NO_WARNINGS)

  # when compiling or linking against GTEST on MSVC2017 the following
  # definition is required: /D _SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING
  add_definitions(-D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING)
endif ()

add_custom_target(iresearch-tests
  DEPENDS iresearch-tests-static
  )

add_executable(iresearch-tests-static
  EXCLUDE_FROM_ALL ${IReSearch_sources}
  ${IReSearch_tests_sources} ${IReSearch_tests_headers}
  ./analysis/text_analyzer_tests.cpp
  ./formats/formats_10_tests.cpp
  ./formats/formats_11_tests.cpp
  ./formats/formats_12_tests.cpp
  ./formats/formats_13_tests.cpp
  ./formats/formats_14_tests.cpp
  ./formats/formats_15_tests.cpp
  )

add_dependencies(iresearch-tests-static
  iresearch-analyzer-delimiter-static
  iresearch-analyzer-multi-delimiter-static
  iresearch-analyzer-ngram-static
  iresearch-analyzer-text-static
  iresearch-analyzer-norm-static
  iresearch-analyzer-stem-static
  iresearch-analyzer-stopwords-static
  iresearch-analyzer-pipeline-static
  iresearch-analyzer-segmentation-static
  iresearch-analyzer-minhash-static
  iresearch-format-1_0-static
  iresearch-scorer-tfidf-static
  iresearch-scorer-bm25-static
  )

target_include_directories(iresearch-tests-static
  PRIVATE ${PROJECT_BINARY_DIR}/core
  )

if (MSVC)
  target_compile_options(iresearch-tests-static
    PRIVATE "/bigobj"
    )
endif ()

set_target_properties(iresearch-tests-static
  PROPERTIES
  OUTPUT_NAME iresearch-tests-s
  )

target_link_libraries(iresearch-tests-static
  ${GCOV_LIBRARY}
  iresearch-static
  ${GTEST_STATIC_LIBS}
  ${PTHREAD_LIBRARY}
  gtest
  )

# setup CRT
if (MSVC)
  target_compile_options(iresearch-tests-static
    PRIVATE "$<$<CONFIG:Debug>:/bigobj>"
    )
endif ()

include_directories(iresearch-tests-static
  ${IResearchTests_INCLUDE_DIR}
  ${IReSearch_generated_INCLUDE_DIR}
  ${IResearch_INCLUDE_DIR}
)
include_directories(iresearch-tests-static SYSTEM
  ${EXTERNAL_INCLUDE_DIRS}
  ${ICU_INCLUDE_DIR}
  ${Fasttext_INCLUDE_DIR}
  ${GTEST_INCLUDE_DIR}
  ${Boost_INCLUDE_DIRS}
  ${FROZEN_INCLUDE_DIR}
  $<TARGET_PROPERTY:iresearch-cmdline,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:iresearch-rapidjson,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:iresearch-ofst,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:iresearch-utfcpp,INTERFACE_INCLUDE_DIRECTORIES>
  )
