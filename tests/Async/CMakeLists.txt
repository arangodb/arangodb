target_sources(arangodbtests
  PRIVATE
  AsyncTest.cpp
  ExpectedTest.cpp)

add_executable(resume_async_via_future EXCLUDE_FROM_ALL
  ResumeAsyncViaFuture.cpp)
target_link_libraries(resume_async_via_future
  PRIVATE
  arango_async_registry
  arango_futures
  arango)
add_executable(infecting_the_stack EXCLUDE_FROM_ALL
  InfectingStack.cpp)
target_link_libraries(infecting_the_stack
  PRIVATE
  arango_async_registry
  arango_futures
  arango)
add_executable(future_vs_async EXCLUDE_FROM_ALL
  /home/jvolmer/docs/notes/async_observability/coroutine_talk/code/future_vs_async.cpp)
target_link_libraries(future_vs_async
  PRIVATE
  arango_async_registry
  arango_futures
  arango)

add_subdirectory(Registry)
