add_library(arango_async_registry STATIC
  promise.cpp
  registry.cpp
  registry_variable.cpp
  thread_registry.cpp)
target_include_directories(arango_async_registry PUBLIC
  ${PROJECT_SOURCE_DIR}/lib)
target_link_libraries(arango_async_registry
  PRIVATE
  arango_assertions
  PUBLIC
  arango_metrics_base)
