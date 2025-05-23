add_library(arango_tests_forest OBJECT
  ForestTest.cpp)
target_link_libraries(arango_tests_forest PRIVATE
  arango_forest
  gtest)

add_executable(arangodbtests_forest EXCLUDE_FROM_ALL)
target_link_libraries(arangodbtests_forest
  arango_tests_forest
  gtest_main)

add_subdirectory(Concurrent)
