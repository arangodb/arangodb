add_library(arango_tests_ast_pretty_printer OBJECT
  PrettyPrinterTest.cpp)

target_link_libraries(arango_tests_ast_pretty_printer PUBLIC
  arango_lightweight
  arango_aql
  velocypack
  arango_crashhandler_light
  arango_futures
  gtest
  gmock
  immer
  rocksdb_interface
  boost_boost
  )

target_include_directories(arango_tests_ast_pretty_printer PUBLIC
        "${PROJECT_SOURCE_DIR}/arangod"
        "${PROJECT_SOURCE_DIR}/tests"
        "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

add_executable(arangodbtests_ast_pretty_printer EXCLUDE_FROM_ALL)

target_link_libraries(arangodbtests_ast_pretty_printer PRIVATE
  velocypack_utils
  arango_tests_basics
  arango_tests_ast_pretty_printer)
