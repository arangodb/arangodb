add_library(arango_tests_datetime OBJECT
  IsoWeekTest.cpp)
target_include_directories(arango_tests_datetime PRIVATE
  ${PROJECT_SOURCE_DIR}/lib)
target_link_libraries(arango_tests_datetime PRIVATE
  arango_datetime
  gtest)

add_executable(arangodbtests_datetime EXCLUDE_FROM_ALL)
target_link_libraries(arangodbtests_datetime
  arango_tests_datetime
  gtest_main)

