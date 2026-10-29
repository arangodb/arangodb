add_library(arango_infra STATIC
  MonitorRegistry.cpp
  ResourceMonitorScope.cpp)

# MallocOverride.cpp is deliberately not part of this library. It must be a
# direct source of each executable that wants it: the linker only pulls .a
# members that resolve undefined symbols, so an interposing malloc can be
# silently dropped, and the override is process-wide, so it must not reach
# binaries that expect the normal allocator.

target_link_libraries(arango_infra
  PUBLIC
  arango_assertions
  arango_basic_utils)

# Consumers include these headers as "infra/...".
target_include_directories(arango_infra PUBLIC
  "${PROJECT_SOURCE_DIR}/arangod")
