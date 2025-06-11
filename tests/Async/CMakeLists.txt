target_sources(arangodbtests
  PRIVATE
  AsyncTest.cpp
  ExpectedTest.cpp)

add_executable(resume_async_via_future
  ResumeAsyncViaFuture.cpp)
target_link_libraries(resume_async_via_future
  PRIVATE
  arango_async_registry
  arango_futures
  arango)
add_executable(infecting_the_stack
  /home/jvolmer/docs/notes/async_observability/coroutine_talk/code/infecting_stack.cpp)
target_link_libraries(infecting_the_stack
  PRIVATE
  arango_async_registry
  arango_futures
  arango)
add_executable(future_vs_async
  /home/jvolmer/docs/notes/async_observability/coroutine_talk/code/future_vs_async.cpp)
target_link_libraries(future_vs_async
  PRIVATE
  arango_async_registry
  arango_futures
  arango)
add_executable(async_on_one_thread
  /home/jvolmer/docs/notes/async_observability/coroutine_talk/code/request.cpp)
target_link_libraries(async_on_one_thread
  PRIVATE
  arango_async_registry
  arango_futures
  arango)
# target_include_directories(async_on_one_thread
# 	PRIVATE
# 	/home/jvolmer/docs/notes/async_observability/coroutine_talk/includes)

add_subdirectory(Registry)
