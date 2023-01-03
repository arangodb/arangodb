add_executable(arangodbtests_actor
  ${PROJECT_SOURCE_DIR}/arangod/Pregel/Actor/Actor.cpp
  main.cpp
  ActorTest.cpp
  RuntimeTest.cpp
  # MultiRuntimeTest.cpp
  MPSCQueueTest.cpp
  SimpleQueueTest.cpp
  )

target_include_directories(arangodbtests_actor PRIVATE
  ${PROJECT_SOURCE_DIR}/lib
  ${PROJECT_BINARY_DIR}/lib
  ${PROJECT_SOURCE_DIR}/arangod)

target_link_libraries(arangodbtests_actor boost_boost velocypack fmt gtest)

# FIXME: setting the working directory manually is a hack
# that needs to be removed/addressed.
# without this hack ctest currently does not find the executable.
add_test(
  NAME mpsc_queue
  COMMAND arangodbtests_actor --gtest_filter=MPSCQueue*
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

add_test(
  NAME simple_queue
  COMMAND arangodbtests_actor --gtest_filter=SimpleQueue*
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

add_test(
  NAME actor
  COMMAND arangodbtests_actor --gtest_filter=Actor*
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
