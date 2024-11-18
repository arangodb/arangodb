add_library(arango_tests_zkd OBJECT
  Conversion.cpp
  Library.cpp)
target_link_libraries(arango_tests_zkd
  PRIVATE
    arango_rocksdb
    gtest)	
target_include_directories(arango_tests_zkd
  PRIVATE
    ${PROJECT_SOURCE_DIR}/arangod)

if (USE_FAST_DOUBLE_MEMCMP_ENCODING)
    target_compile_definitions(arango_tests_zkd PRIVATE USE_FAST_DOUBLE_MEMCMP_ENCODING)
endif ()

add_executable(arangodbtests_zkd EXCLUDE_FROM_ALL)
target_link_libraries(arangodbtests_zkd
  arango_tests_zkd
  gtest_main)

add_test(NAME zkd
         COMMAND arangodbtests_zkd)
