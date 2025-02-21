add_library(arango_async INTERFACE)
target_include_directories(arango_async
  INTERFACE
  include
)

target_link_libraries(arango_async
  INTERFACE
  arango_async_registry
  arango_async_interface
  arango_basic_utils
)

add_subdirectory(include)
add_subdirectory(Registry)
