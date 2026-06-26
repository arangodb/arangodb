add_library(arango_tests_futures OBJECT
  FutureTest.cpp
  FutureCoroutineTest.cpp
  PromiseTest.cpp
  TryTest.cpp)
target_link_libraries(arango_tests_futures
  PRIVATE
    arango
    arango_futures
    gtest
    gmock
    velocypack)
target_include_directories(arango_tests_futures PRIVATE
  ${PROJECT_SOURCE_DIR}/arangod
  ${PROJECT_SOURCE_DIR}/tests
  ${PROJECT_SOURCE_DIR}/tests/Mocks/)

add_executable(arangodbtests_futures EXCLUDE_FROM_ALL)
target_link_libraries(arangodbtests_futures
  arango_crashhandler_light
  gtest_main
  arango_tests_futures)

add_test(NAME futures
         COMMAND arangodbtests_futures)
