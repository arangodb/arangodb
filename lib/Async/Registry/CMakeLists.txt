add_library(arango_async_registry STATIC
  promise.cpp
  registry_variable.cpp)
target_include_directories(arango_async_registry PRIVATE
  ${PROJECT_SOURCE_DIR}/arangod)
target_link_libraries(arango_async_registry
  PRIVATE
  arango_thread_owning_list
  arango_assertions
  PUBLIC
  arango_metrics_base)
