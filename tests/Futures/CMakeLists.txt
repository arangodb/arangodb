add_library(arango_tests_futures OBJECT
  FutureTest.cpp
  PromiseTest.cpp
  TryTest.cpp
  main.cpp)

target_include_directories(arango_tests_futures PRIVATE
  ${PROJECT_SOURCE_DIR}/3rdParty/iresearch/external/function2)

target_link_libraries(arango_tests_futures
    arango_futures
    gtest
    velocypack
    fmt)

add_executable(arangodbtests_futures
  EXCLUDE_FROM_ALL)

target_link_libraries(arangodbtests_futures
    gtest
    arango_tests_futures)

add_test(NAME futures
         COMMAND arangodbtests_futures)
