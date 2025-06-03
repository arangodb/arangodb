target_sources(arangodbtests
  PRIVATE
  AsyncTest.cpp
  ExpectedTest.cpp)

add_executable(async_vs_future
  AsyncVsFuture.cpp)
target_link_libraries(async_vs_future
  PRIVATE
  arango_async_registry
  arango_futures
  arango)

add_subdirectory(Registry)
