# note that typedbuffer.cpp is excluded intentionally
# since it seems to have a big dependency footprint;
# we intent to remove the TypedBuffer construct soon
add_library(arango_tests_pregel OBJECT
  DurationTest.cpp
  StatusTest.cpp)

target_link_libraries(arango_tests_pregel
    arango_tests_basics
    velocypack
    fmt)

add_executable(arangodbtests_pregel
  EXCLUDE_FROM_ALL)

target_link_libraries(arangodbtests_pregel
    gtest_main
    arango_tests_pregel)

add_test(NAME pregel
         COMMAND arangodbtests_pregel)
