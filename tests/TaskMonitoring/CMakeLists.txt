add_library(arango_tests_activity_registry OBJECT
  ActivityRegistryTest.cpp)
target_link_libraries(arango_tests_activity_registry PRIVATE
  arango_activity_registry_global
  arango_async
  gtest
)

add_executable(arangodbtests_activity_registry EXCLUDE_FROM_ALL)
target_link_libraries(arangodbtests_activity_registry
  arango_tests_activity_registry
  arango_async
  gtest_main)

