add_library(arango_async_interface INTERFACE)
target_include_directories(arango_async_interface INTERFACE
  ${PROJECT_SOURCE_DIR}/lib
  include)
target_link_libraries(arango_async_interface INTERFACE
  arango_activity_registry_interface)

add_library(arango_async STATIC
  include/Async/async.h
  include/Async/coro-utils.h
  include/Async/expected.h
  include/Async/SuspensionCounter.h
  src/SuspensionCounter.cpp
)
target_include_directories(arango_async 
  PUBLIC
    include
  PRIVATE
    "${PROJECT_SOURCE_DIR}/arangod")
target_link_libraries(arango_async INTERFACE
  arango_async_interface
  arango_activity_registry_interface
)
target_link_libraries(arango_async PUBLIC
  arango_inspection
  arango_async_registry
  arango_basic_utils
)

add_subdirectory(Registry)
