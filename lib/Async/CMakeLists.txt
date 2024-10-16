add_library(arango_async INTERFACE)
target_include_directories(arango_async
  INTERFACE
  include)
target_link_libraries(arango_async
  INTERFACE
  arango_utils
  arango_async_registry)

add_subdirectory(Registry)
