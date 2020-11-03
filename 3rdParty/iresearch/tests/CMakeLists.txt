set (IResearchTests_TARGET_NAME 
  "iresearch-tests"
  CACHE INTERNAL
  ""
)

find_package(GTestLocal
  REQUIRED
)

SOURCE_GROUP("analysis" ./analysis/*)
SOURCE_GROUP("store" ./store/*)
SOURCE_GROUP("index" ./index/*)
SOURCE_GROUP("formats" ./formats/*)
SOURCE_GROUP("search" ./search/*)
SOURCE_GROUP("iql" ./iql/*)
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
  ./analysis/ngram_token_stream_test.cpp
  ./analysis/pipeline_stream_tests.cpp
  ./analysis/text_token_normalizing_stream_tests.cpp
  ./analysis/text_token_stemming_stream_tests.cpp
  ./analysis/token_masking_stream_tests.cpp
  ./analysis/token_attributes_test.cpp
  ./analysis/token_stream_tests.cpp
  ./formats/formats_tests.cpp
  ./formats/formats_test_case_base.cpp
  ./formats/skip_list_test.cpp
  ./store/directory_test_case.cpp
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
  ./index/sorted_index_tests.cpp
  ./index/index_death_tests.cpp
  ./index/field_meta_test.cpp
  ./index/merge_writer_tests.cpp
  ./index/postings_tests.cpp
  ./index/sorted_column_test.cpp
  ./index/segment_writer_tests.cpp
  ./index/consolidation_policy_tests.cpp
  ./search/empty_filter_tests.cpp
  ./search/granular_range_filter_tests.cpp
  ./search/wildcard_filter_test.cpp
  ./search/levenshtein_filter_test.cpp
  ./search/index_reader_test.cpp
  ./search/scorers_tests.cpp
  ./search/bitset_doc_iterator_test.cpp
  ./search/sort_tests.cpp
  ./search/tfidf_test.cpp
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
  ./iql/parser_common_test.cpp
  ./iql/query_builder_test.cpp
  ./utils/async_utils_tests.cpp
  ./utils/automaton_test.cpp
  ./utils/bitvector_tests.cpp
  ./utils/container_utils_tests.cpp
  ./utils/compression_test.cpp
  ./utils/crc_test.cpp
  ./utils/file_utils_tests.cpp
  ./utils/map_utils_tests.cpp
  ./utils/object_pool_tests.cpp
  ./utils/numeric_utils_test.cpp
  ./utils/attributes_tests.cpp
  ./utils/directory_utils_tests.cpp
  ./utils/bit_packing_tests.cpp
  ./utils/bit_utils_tests.cpp
  ./utils/block_pool_test.cpp
  ./utils/encryption_test.cpp
  ./utils/locale_utils_tests.cpp
  ./utils/levenshtein_utils_test.cpp
  ./utils/wildcard_utils_test.cpp
  ./utils/ref_counter_tests.cpp
  ./utils/memory_tests.cpp
  ./utils/string_tests.cpp
  ./utils/bitset_tests.cpp
  ./utils/ebo_tests.cpp
  ./utils/math_utils_test.cpp
  ./utils/std_test.cpp
  ./utils/type_utils_tests.cpp
  ./utils/utf8_utils_test.cpp
  ./utils/utf8_path_tests.cpp
  ./utils/fst_builder_test.cpp
  ./utils/fst_utils_test.cpp
  ./utils/fst_string_weight_test.cpp
  ./utils/ngram_match_utils_tests.cpp
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
endif()

add_custom_target(${IResearchTests_TARGET_NAME}
  DEPENDS ${IResearchTests_TARGET_NAME}-shared ${IResearchTests_TARGET_NAME}-static
)

add_executable(${IResearchTests_TARGET_NAME}-shared
  EXCLUDE_FROM_ALL ${IReSearch_sources}
  ${IReSearch_tests_sources} ${IReSearch_tests_headers}
  $<TARGET_OBJECTS:${IResearch_TARGET_NAME}-ofst>
)

set_ipo(${IResearchTests_TARGET_NAME}-shared)
add_sanitizers(${IResearchTests_TARGET_NAME}-shared)

add_executable(${IResearchTests_TARGET_NAME}-static
  EXCLUDE_FROM_ALL ${IReSearch_sources}
  ${IReSearch_tests_sources} ${IReSearch_tests_headers}
  ./analysis/text_analyzer_tests.cpp
  ./formats/formats_10_tests.cpp
  ./formats/formats_11_tests.cpp
  ./formats/formats_12_tests.cpp
  ./formats/formats_13_tests.cpp
  ./iql/parser_test.cpp
)

set_ipo(${IResearchTests_TARGET_NAME}-static)
add_sanitizers(${IResearchTests_TARGET_NAME}-static)


add_dependencies(${IResearchTests_TARGET_NAME}-shared
  ${IResearch_TARGET_NAME}-analyzer-delimiter-shared
  ${IResearch_TARGET_NAME}-analyzer-ngram-shared
  ${IResearch_TARGET_NAME}-analyzer-text-shared
  ${IResearch_TARGET_NAME}-analyzer-norm-shared
  ${IResearch_TARGET_NAME}-analyzer-stem-shared
  ${IResearch_TARGET_NAME}-analyzer-mask-shared
  ${IResearch_TARGET_NAME}-format-1_0-shared
  ${IResearch_TARGET_NAME}-scorer-tfidf-shared
  ${IResearch_TARGET_NAME}-scorer-bm25-shared
)

add_dependencies(${IResearchTests_TARGET_NAME}-static
  ${IResearch_TARGET_NAME}-analyzer-delimiter-static
  ${IResearch_TARGET_NAME}-analyzer-ngram-static
  ${IResearch_TARGET_NAME}-analyzer-text-static
  ${IResearch_TARGET_NAME}-analyzer-norm-static
  ${IResearch_TARGET_NAME}-analyzer-stem-static
  ${IResearch_TARGET_NAME}-analyzer-mask-static
  ${IResearch_TARGET_NAME}-format-1_0-static
  ${IResearch_TARGET_NAME}-scorer-tfidf-static
  ${IResearch_TARGET_NAME}-scorer-bm25-static
)

target_include_directories(${IResearchTests_TARGET_NAME}-shared
  PRIVATE ${PROJECT_BINARY_DIR}/core
)

target_include_directories(${IResearchTests_TARGET_NAME}-static
  PRIVATE ${PROJECT_BINARY_DIR}/core
)

if(MSVC)
  target_compile_options(${IResearchTests_TARGET_NAME}-shared
    PRIVATE "/bigobj"
  )

  target_compile_options(${IResearchTests_TARGET_NAME}-static
    PRIVATE "/bigobj"
  )
endif()

set_target_properties(${IResearchTests_TARGET_NAME}-shared
  PROPERTIES
  OUTPUT_NAME iresearch-tests
  COMPILE_DEFINITIONS "$<$<CONFIG:Coverage>:IRESEARCH_DEBUG>;$<$<CONFIG:Debug>:IRESEARCH_DEBUG>;FST_NO_DYNAMIC_LINKING;IRESEARCH_DLL;BOOST_ALL_DYN_LINK"
)

set_target_properties(${IResearchTests_TARGET_NAME}-static
  PROPERTIES
  OUTPUT_NAME iresearch-tests-s
  COMPILE_DEFINITIONS "$<$<CONFIG:Coverage>:IRESEARCH_DEBUG>;$<$<CONFIG:Debug>:IRESEARCH_DEBUG>;FST_NO_DYNAMIC_LINKING"
)

target_link_libraries(${IResearchTests_TARGET_NAME}-shared
  ${GCOV_LIBRARY}
  ${IResearch_TARGET_NAME}-shared
  ${IResearch_TARGET_NAME}-analyzer-delimiter-shared
  ${IResearch_TARGET_NAME}-analyzer-ngram-shared
  ${IResearch_TARGET_NAME}-analyzer-text-shared
  ${IResearch_TARGET_NAME}-analyzer-norm-shared
  ${IResearch_TARGET_NAME}-analyzer-stem-shared
  ${IResearch_TARGET_NAME}-analyzer-mask-shared
  ${IResearch_TARGET_NAME}-format-1_0-shared
  ${GTEST_STATIC_LIBS}
  ${PTHREAD_LIBRARY}
)

target_link_libraries(${IResearchTests_TARGET_NAME}-static
  ${GCOV_LIBRARY}
  ${IResearch_TARGET_NAME}-static-allinone
  ${GTEST_STATIC_LIBS}
  ${PTHREAD_LIBRARY}
)

# setup CRT
if(MSVC)
  target_compile_options(${IResearchTests_TARGET_NAME}-shared
    PRIVATE "$<$<CONFIG:Debug>:/bigobj>"
  )

  target_compile_options(${IResearchTests_TARGET_NAME}-static
    PRIVATE "$<$<CONFIG:Debug>:/bigobj>"
  )
endif()

include_directories(${IResearchTests_TARGET_NAME}-shared
  ${IResearchTests_INCLUDE_DIR}
  ${IReSearch_generated_INCLUDE_DIR}
  ${IResearch_INCLUDE_DIR}
  ${ICU_INCLUDE_DIR}
  ${GTEST_INCLUDE_DIR}
  ${Boost_INCLUDE_DIRS}
  ${FROZEN_INCLUDE_DIR}
  $<TARGET_PROPERTY:${IResearch_TARGET_NAME}-ofst,INTERFACE_INCLUDE_DIRECTORIES>
)

include_directories(${IResearchTests_TARGET_NAME}-static
  ${IResearchTests_INCLUDE_DIR}
  ${IReSearch_generated_INCLUDE_DIR}
  ${IResearch_INCLUDE_DIR}
  ${ICU_INCLUDE_DIR}
  ${GTEST_INCLUDE_DIR}
  ${Boost_INCLUDE_DIRS}
  ${FROZEN_INCLUDE_DIR}
  $<TARGET_PROPERTY:${IResearch_TARGET_NAME}-cmdline,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:${IResearch_TARGET_NAME}-rapidjson,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:${IResearch_TARGET_NAME}-ofst,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:${IResearch_TARGET_NAME}-utfcpp,INTERFACE_INCLUDE_DIRECTORIES>
)

################################################################################
### @brief copy Boost shared dependencies
################################################################################
foreach(ELEMENT ${Boost_SHARED_sharedRT_LIB_RESOURCES})
  if (APPLE)
    set(CP_OPTS "-f") # MacOS does not support hard-linking
  else()
    set(CP_OPTS "-lf")
  endif()

  add_custom_command(
    TARGET ${IResearchTests_TARGET_NAME}-shared POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "copying library resource:" "${ELEMENT}" " -> " "$<TARGET_FILE_DIR:${IResearchTests_TARGET_NAME}-shared>"
    COMMAND cp ${CP_OPTS} ${ELEMENT} $<TARGET_FILE_DIR:${IResearchTests_TARGET_NAME}-shared> || ${CMAKE_COMMAND} -E copy ${ELEMENT} $<TARGET_FILE_DIR:${IResearchTests_TARGET_NAME}-shared>
  )
endforeach()

################################################################################
### @brief copy ICU shared dependencies
################################################################################
foreach(ELEMENT ${ICU_SHARED_LIB_RESOURCES})
  if (APPLE)
    set(CP_OPTS "-f") # MacOS does not support hard-linking
  else()
    set(CP_OPTS "-lf")
  endif()

  add_custom_command(
    TARGET ${IResearchTests_TARGET_NAME}-shared POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "copying library resource:" "${ELEMENT}" " -> " "$<TARGET_FILE_DIR:${IResearchTests_TARGET_NAME}-shared>"
    COMMAND cp ${CP_OPTS} ${ELEMENT} $<TARGET_FILE_DIR:${IResearchTests_TARGET_NAME}-shared> || ${CMAKE_COMMAND} -E copy ${ELEMENT} $<TARGET_FILE_DIR:${IResearchTests_TARGET_NAME}-shared>
  )
endforeach()

################################################################################
### @brief copy BFD shared dependencies
################################################################################
foreach(ELEMENT ${BFD_SHARED_LIB_RESOURCES})
  if (APPLE)
    set(CP_OPTS "-f") # MacOS does not support hard-linking
  else()
    set(CP_OPTS "-lf")
  endif()

  add_custom_command(
    TARGET ${IResearchTests_TARGET_NAME}-shared POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "copying library resource:" "${ELEMENT}" " -> " "$<TARGET_FILE_DIR:${IResearchTests_TARGET_NAME}-shared>"
    COMMAND cp ${CP_OPTS} ${ELEMENT} $<TARGET_FILE_DIR:${IResearchTests_TARGET_NAME}-shared> || ${CMAKE_COMMAND} -E copy ${ELEMENT} $<TARGET_FILE_DIR:${IResearchTests_TARGET_NAME}-shared>
  )
endforeach()

################################################################################
### @brief copy Unwind shared dependencies
################################################################################
foreach(ELEMENT ${Unwind_SHARED_LIB_RESOURCES})
  if (APPLE)
    set(CP_OPTS "-f") # MacOS does not support hard-linking
  else()
    set(CP_OPTS "-lf")
  endif()

  add_custom_command(
    TARGET ${IResearchTests_TARGET_NAME}-shared POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "copying library resource:" "${ELEMENT}" " -> " "$<TARGET_FILE_DIR:${IResearchTests_TARGET_NAME}-shared>"
    COMMAND cp ${CP_OPTS} ${ELEMENT} $<TARGET_FILE_DIR:${IResearchTests_TARGET_NAME}-shared> || ${CMAKE_COMMAND} -E copy ${ELEMENT} $<TARGET_FILE_DIR:${IResearchTests_TARGET_NAME}-shared>
  )
endforeach()
