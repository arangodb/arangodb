# note that typedbuffer.cpp is excluded intentionally
# since it seems to have a big dependency footprint;
# we intent to remove the TypedBuffer construct soon
add_library(arango_tests_pregel OBJECT
  DurationTest.cpp
  PregelShardTest.cpp
  StatusTest.cpp)

target_include_directories(arango_tests_pregel
  PRIVATE
  ${PROJECT_SOURCE_DIR}/arangod
  ${PROJECT_SOURCE_DIR}/lib)

target_link_libraries(arango_tests_pregel
  PRIVATE
  gtest
  date_interface
  velocypack
  fmt)

add_executable(arangodbtests_pregel
  EXCLUDE_FROM_ALL)

target_link_libraries(arangodbtests_pregel
    gtest_main
    arango_tests_pregel
    arango_crashhandler_light
    arango_assertions)

add_test(NAME pregel
         COMMAND arangodbtests_pregel)
