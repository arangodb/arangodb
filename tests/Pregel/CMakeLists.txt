add_library(arango_tests_pregel OBJECT
  ConductorStateTest.cpp
  DurationTest.cpp
  PregelShardTest.cpp
  StatusTest.cpp)

target_include_directories(arango_tests_pregel
  PRIVATE
  ${PROJECT_SOURCE_DIR}/arangod
  ${PROJECT_SOURCE_DIR}/lib)

target_link_libraries(arango_tests_pregel
  PRIVATE
  arango_inspection
  gtest
  arango_actor
  arango_pregel_with_actors
  date_interface
  velocypack
  fmt c++experimental)

add_executable(arangodbtests_pregel
  EXCLUDE_FROM_ALL)

target_link_libraries(arangodbtests_pregel
    gtest_main
    arango_tests_pregel
    arango_tests_pregel_graphstore
    arango_crashhandler_light
    arango_assertions)

add_test(NAME pregel
  COMMAND arangodbtests_pregel)

add_subdirectory(Actor)
add_subdirectory(GraphStore)
