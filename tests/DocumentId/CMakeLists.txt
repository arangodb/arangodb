add_library(arango_tests_documentid OBJECT
  DocumentIdBackingStoreTest.cpp
  DocumentIdTest.cpp)
target_include_directories(arango_tests_documentid PRIVATE
  ${PROJECT_SOURCE_DIR}/lib)
target_link_libraries(arango_tests_documentid PRIVATE
  arango_documentid_standalone
  arango_inspection
  arango_lightweight
  gtest
  velocypack_utils)

add_executable(arangodbtests_documentid EXCLUDE_FROM_ALL)
target_link_libraries(arangodbtests_documentid
  arango_tests_documentid
  gtest_main)

add_test(
  NAME actor
  COMMAND arangodbtests_documentid)
