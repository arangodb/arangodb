add_library(arango_tests_actor OBJECT
  ActorListTest.cpp
  ActorTest.cpp
  EgressActorTest.cpp
  RuntimeTest.cpp
  MultiRuntimeTest.cpp
  MPSCQueueTest.cpp)
target_include_directories(arango_tests_actor PRIVATE
  ${PROJECT_SOURCE_DIR}/lib)
target_link_libraries(arango_tests_actor PRIVATE
  arango_actor_standalone
  arango_inspection
  gtest
  velocypack_utils)

add_executable(arangodbtests_actor EXCLUDE_FROM_ALL)
target_link_libraries(arangodbtests_actor
  arango_tests_actor
  gtest_main)

add_test(
  NAME actor
  COMMAND arangodbtests_actor)
