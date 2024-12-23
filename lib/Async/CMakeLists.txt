add_library(arango_async STATIC
  include/Async/async.h
  include/Async/coro-utils.h
  include/Async/expected.h
  include/Async/SuspensionSemaphore.h
  src/SuspensionSemaphore.cpp
)
target_include_directories(arango_async
  INTERFACE
  include)
target_link_libraries(arango_async
  INTERFACE
  arango_utils
  arango_async_registry)

add_subdirectory(Registry)
