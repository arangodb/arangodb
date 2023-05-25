add_library(arango_tests_pregel_graphstore OBJECT
  MemoryResourceTest.cpp
#  QuiverTest.cpp
)

target_include_directories(arango_tests_pregel_graphstore
  PRIVATE
  ${PROJECT_SOURCE_DIR}/arangod
  ${PROJECT_SOURCE_DIR}/lib)

target_link_libraries(arango_tests_pregel_graphstore
  PRIVATE
  arango_inspection
  gtest
  velocypack
  fmt)

add_executable(arangodbtests_pregel_graphstore
  EXCLUDE_FROM_ALL)

target_link_libraries(arangodbtests_pregel_graphstore
    gtest_main
    arango_tests_pregel_graphstore
    arango_crashhandler_light
    arango_assertions)

add_test(NAME pregel_graphstore
         COMMAND arangodbtests_pregel_graphstore)
