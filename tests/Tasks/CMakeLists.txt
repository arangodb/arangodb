add_executable(task_registry_tests EXCLUDE_FROM_ALL
  TaskRegistryTest.cpp)
target_link_libraries(task_registry_tests
  arango_task_registry_global
  gtest_main)
