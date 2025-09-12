add_library(arango_tests_task_registry OBJECT
  TaskRegistryTest.cpp)
target_link_libraries(arango_tests_task_registry PRIVATE
  arango_task_registry_global
  arango_async
  gtest
)

add_executable(arangodbtests_task_registry EXCLUDE_FROM_ALL)
target_link_libraries(arangodbtests_task_registry
  arango_tests_task_registry
  arango_async
  gtest_main)

