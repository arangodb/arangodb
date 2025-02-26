add_executable(task_registry_tests EXCLUDE_FROM_ALL
  TaskRegistryTest.cpp)
target_link_libraries(task_registry_tests PRIVATE
  arango_task_registry
  gtest_main)
