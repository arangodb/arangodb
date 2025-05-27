add_library(arango_async_interface INTERFACE)
target_include_directories(arango_async_interface INTERFACE
  ${PROJECT_SOURCE_DIR}/lib
  include)
target_link_libraries(arango_async_interface INTERFACE
  arango_task_registry)

add_library(arango_async INTERFACE)
target_include_directories(arango_async INTERFACE
  include)

target_link_libraries(arango_async INTERFACE
  arango_async_registry
  arango_async_interface
  arango_basic_utils
)

add_subdirectory(Registry)
