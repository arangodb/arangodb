set (IResearchTests_TARGET_NAME 
  "iresearch-tests"
  CACHE INTERNAL
  ""
)

# find Google Test Framework
if("${GTEST_ROOT}" STREQUAL "")
  if(NOT "$ENV{GTEST_ROOT}" STREQUAL "")
    set(GTEST_ROOT "$ENV{GTEST_ROOT}")
  else()
    if(NOT MSVC AND EXISTS "/usr/include/gtest/gtest.h")
      set(GTEST_ROOT "/usr")
    endif()
  endif()
endif()
find_package(GTest
  REQUIRED
)

# set rapidjson directories
set(RAPIDJSON_INCLUDE_DIR
  ${EXTERNAL_INCLUDE_DIRS}/rapidjson
)

# set cmdline directories
set(CMDLINE_INCLUDE_DIR
  ${EXTERNAL_INCLUDE_DIRS}/cmdline
)

# set test resource directory
set(IResearch_test_resource_dir
  ${CMAKE_CURRENT_SOURCE_DIR}/resources
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

configure_file( 
  "${CMAKE_CURRENT_SOURCE_DIR}/tests_config.hpp.in" 
  "${IReSearch_generated_INCLUDE_DIR}/tests_config.hpp" 
)

set(IReSearch_tests_sources
  ./analysis/analyzer_test.cpp
  ./analysis/token_stream_tests.cpp
  ./formats/formats_tests.cpp
  ./formats/skip_list_test.cpp
  ./store/directory_test_case.cpp
  ./store/directory_cleaner_tests.cpp
  ./store/fs_directory_tests.cpp
  ./store/memory_directory_tests.cpp
  ./store/memory_index_output_tests.cpp
  ./store/store_utils_tests.cpp
  ./index/doc_generator.cpp
  ./index/assert_format.cpp
  ./index/index_meta_tests.cpp
  ./index/index_tests.cpp
  ./index/field_meta_test.cpp
  ./index/merge_writer_tests.cpp
  ./index/postings_tests.cpp
  ./search/empty_filter_tests.cpp
  ./search/granular_range_filter_tests.cpp
  ./search/index_reader_test.cpp
  ./search/scorers_tests.cpp
  ./search/sort_tests.cpp
  ./search/tfidf_test.cpp
  ./search/bm25_test.cpp
  ./search/cost_attribute_test.cpp
  ./search/boost_attribute_test.cpp
  ./search/filter_test_case_base.cpp
  ./search/boolean_filter_tests.cpp
  ./search/all_filter_tests.cpp
  ./search/term_filter_tests.cpp
  ./search/prefix_filter_test.cpp
  ./search/range_filter_test.cpp
  ./search/phrase_filter_tests.cpp
  ./search/column_existence_filter_test.cpp
  ./search/same_position_filter_tests.cpp
  ./iql/parser_common_test.cpp
  ./iql/query_builder_test.cpp
  ./utils/async_utils_tests.cpp
  ./utils/container_utils_tests.cpp
  ./utils/map_utils_tests.cpp
  ./utils/object_pool_tests.cpp
  ./utils/numeric_utils_test.cpp
  ./utils/attributes_tests.cpp
  ./utils/directory_utils_tests.cpp
  ./utils/bit_packing_tests.cpp
  ./utils/bit_utils_tests.cpp
  ./utils/block_pool_test.cpp
  ./utils/locale_utils_tests.cpp
  ./utils/ref_counter_tests.cpp
  ./utils/memory_tests.cpp
  ./utils/string_tests.cpp
  ./utils/bitset_tests.cpp
  ./utils/ebo_tests.cpp
  ./utils/math_utils_test.cpp
  ./utils/std_test.cpp
  ./utils/type_utils_tests.cpp
  ./utils/utf8_path_tests.cpp
  ./tests_main.cpp
)

set(IReSearch_tests_headers
  ./store/directory_test_case.hpp
  ./index/assert_format.hpp
  ./index/doc_generator.hpp
  ./index/index_tests.hpp
  ./search/filter_test_case_base.hpp
  ./formats/formats_test_case_base.hpp
  ./unicode/utf8.h
  ./unicode/utf8/checked.h
  ./unicode/utf8/unchecked.h
  ./unicode/utf8/core.h
  ./tests_config.hpp
  ${IReSearch_generated_INCLUDE_DIR}/tests_config.hpp # auto generated
)

if (MSVC)
  # disable "checked iterators" feature
  add_definitions(-D_SCL_SECURE_NO_WARNINGS) 
endif()

add_custom_target(${IResearchTests_TARGET_NAME}
  DEPENDS ${IResearchTests_TARGET_NAME}-shared ${IResearchTests_TARGET_NAME}-static
)

add_executable(${IResearchTests_TARGET_NAME}-shared
  EXCLUDE_FROM_ALL ${IReSearch_sources}
  ${IReSearch_tests_sources} ${IReSearch_tests_headers}
)

add_executable(${IResearchTests_TARGET_NAME}-static
  EXCLUDE_FROM_ALL ${IReSearch_sources}
  ${IReSearch_tests_sources} ${IReSearch_tests_headers}
  ./analysis/text_analyzer_tests.cpp
  ./formats/formats_10_tests.cpp
  ./iql/parser_test.cpp
)

add_dependencies(${IResearchTests_TARGET_NAME}-shared
  ${IResearch_TARGET_NAME}-analyzer-text-shared
  ${IResearch_TARGET_NAME}-format-1_0-shared
  ${IResearch_TARGET_NAME}-scorer-tfidf-shared
  ${IResearch_TARGET_NAME}-scorer-bm25-shared
)

add_dependencies(${IResearchTests_TARGET_NAME}-static
  ${IResearch_TARGET_NAME}-analyzer-text-static
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

set_target_properties(${IResearchTests_TARGET_NAME}-shared
  PROPERTIES
  OUTPUT_NAME iresearch-tests
  COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>;IRESEARCH_DLL;BOOST_ALL_DYN_LINK"
)

set_target_properties(${IResearchTests_TARGET_NAME}-static
  PROPERTIES
  OUTPUT_NAME iresearch-tests-s
  COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>"
)

target_link_libraries(${IResearchTests_TARGET_NAME}-shared
  ${GCOV_LIBRARY}
  ${IResearch_TARGET_NAME}-shared
  ${IResearch_TARGET_NAME}-analyzer-text-shared
  ${IResearch_TARGET_NAME}-format-1_0-shared
  ${GTEST_BOTH_LIBRARIES}
  ${PTHREAD_LIBRARY}
)

target_link_libraries(${IResearchTests_TARGET_NAME}-static
  ${GCOV_LIBRARY}
  ${IResearch_TARGET_NAME}-static
  ${GTEST_BOTH_LIBRARIES}
  ${PTHREAD_LIBRARY}
)

target_compile_features(${IResearchTests_TARGET_NAME}-shared
  PRIVATE
  cxx_final
  cxx_variadic_templates
)

target_compile_features(${IResearchTests_TARGET_NAME}-static
  PRIVATE
  cxx_final
  cxx_variadic_templates
)

# setup CRT
if(MSVC)
  target_compile_options(${IResearchTests_TARGET_NAME}-shared
    PRIVATE "$<$<CONFIG:Debug>:/MDd>$<$<NOT:$<CONFIG:Debug>>:/MD>"
  )

  target_compile_options(${IResearchTests_TARGET_NAME}-static
    PRIVATE "$<$<CONFIG:Debug>:/MDd>$<$<NOT:$<CONFIG:Debug>>:/MD>"
  )
endif()

include_directories(${IResearchTests_TARGET_NAME}-shared
  ${IResearchTests_INCLUDE_DIR}
  ${IReSearch_generated_INCLUDE_DIR}
  ${IResearch_INCLUDE_DIR}
  ${ICU_INCLUDE_DIR}
  ${GTEST_INCLUDE_DIR}
  ${Boost_INCLUDE_DIRS}
)

include_directories(${IResearchTests_TARGET_NAME}-static
  ${IResearchTests_INCLUDE_DIR}
  ${IReSearch_generated_INCLUDE_DIR}
  ${IResearch_INCLUDE_DIR}
  ${ICU_INCLUDE_DIR}
  ${GTEST_INCLUDE_DIR}
  ${Boost_INCLUDE_DIRS}
  ${RAPIDJSON_INCLUDE_DIR}
  ${CMDLINE_INCLUDE_DIR}
)

################################################################################
### @brief copy Boost shared dependencies
################################################################################
foreach(ELEMENT ${Boost_SHARED_sharedRT_LIB_RESOURCES})
  add_custom_command(
    TARGET ${IResearchTests_TARGET_NAME}-shared POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "copying library resource:" "${ELEMENT}" " -> " "$<TARGET_FILE_DIR:${IResearchTests_TARGET_NAME}-shared>"
    COMMAND cp -lf ${ELEMENT} $<TARGET_FILE_DIR:${IResearchTests_TARGET_NAME}-shared> || ${CMAKE_COMMAND} -E copy ${ELEMENT} $<TARGET_FILE_DIR:${IResearchTests_TARGET_NAME}-shared>
  )
endforeach()

################################################################################
### @brief copy ICU shared dependencies
################################################################################
foreach(ELEMENT ${ICU_SHARED_LIB_RESOURCES})
  add_custom_command(
    TARGET ${IResearchTests_TARGET_NAME}-shared POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "copying library resource:" "${ELEMENT}" " -> " "$<TARGET_FILE_DIR:${IResearchTests_TARGET_NAME}-shared>"
    COMMAND cp -lf ${ELEMENT} $<TARGET_FILE_DIR:${IResearchTests_TARGET_NAME}-shared> || ${CMAKE_COMMAND} -E copy ${ELEMENT} $<TARGET_FILE_DIR:${IResearchTests_TARGET_NAME}-shared>
  )
endforeach()

################################################################################
### @brief copy BFD shared dependencies
################################################################################
foreach(ELEMENT ${BFD_SHARED_LIB_RESOURCES})
  add_custom_command(
    TARGET ${IResearchTests_TARGET_NAME}-shared POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "copying library resource:" "${ELEMENT}" " -> " "$<TARGET_FILE_DIR:${IResearchTests_TARGET_NAME}-shared>"
    COMMAND cp -lf ${ELEMENT} $<TARGET_FILE_DIR:${IResearchTests_TARGET_NAME}-shared> || ${CMAKE_COMMAND} -E copy ${ELEMENT} $<TARGET_FILE_DIR:${IResearchTests_TARGET_NAME}-shared>
  )
endforeach()

################################################################################
### @brief copy Unwind shared dependencies
################################################################################
foreach(ELEMENT ${Unwind_SHARED_LIB_RESOURCES})
  add_custom_command(
    TARGET ${IResearchTests_TARGET_NAME}-shared POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "copying library resource:" "${ELEMENT}" " -> " "$<TARGET_FILE_DIR:${IResearchTests_TARGET_NAME}-shared>"
    COMMAND cp -lf ${ELEMENT} $<TARGET_FILE_DIR:${IResearchTests_TARGET_NAME}-shared> || ${CMAKE_COMMAND} -E copy ${ELEMENT} $<TARGET_FILE_DIR:${IResearchTests_TARGET_NAME}-shared>
  )
endforeach()
