# Allocator-agnostic tests: they run both in the combined arangodbtests binary
# and, below, against the live malloc override.
add_library(arango_tests_infra OBJECT
  AllocHeaderTest.cpp
  MonitorRegistryTest.cpp
  ResizeAccountingTest.cpp
  ResourceMonitorRefTest.cpp
  ResourceMonitorScopeTest.cpp
  ResourceMonitorTest.cpp
  StaticResourceMonitorTest.cpp)

target_link_libraries(arango_tests_infra
  PRIVATE
    arango_infra
    gtest)

# MallocSymmetryTest is not in the object library: it needs the override, which
# is process-wide, so it must not reach the combined binary.
if (USE_MEMORY_TRACKING AND USE_MAINTAINER_MODE)
  add_executable(arangodbtests_memtracking
    EXCLUDE_FROM_ALL
    MallocSymmetryTest.cpp
    ${PROJECT_SOURCE_DIR}/arangod/infra/MallocOverride.cpp)

  target_link_libraries(arangodbtests_memtracking
    arango_tests_infra
    arango_infra
    jemalloc
    arango_crashhandler_light
    gtest
    gtest_main)

  add_test(NAME arangodbtests_memtracking
    COMMAND arangodbtests_memtracking)
endif ()
