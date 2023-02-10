add_library(arango_tests_pregel_store OBJECT
  StoreTest.cpp)

target_include_directories(arango_tests_pregel_store
  PRIVATE
  ${PROJECT_SOURCE_DIR}/arangod
  ${PROJECT_SOURCE_DIR}/lib)

target_link_libraries(arango_tests_pregel_store
  PRIVATE
  gtest
  velocypack
  fmt)

add_executable(arangodbtests_pregel_store
  EXCLUDE_FROM_ALL)

target_link_libraries(arangodbtests_pregel_store
    gtest_main
    arango_tests_pregel_store
    arango_crashhandler_light
    arango_assertions)

add_test(NAME pregel_store
         COMMAND arangodbtests_pregel_store)
