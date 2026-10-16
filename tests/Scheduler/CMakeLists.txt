option(BUILD_SCHEDULER_PERF_TEST "Build Scheduler Perf Test Executable" OFF)

if (BUILD_SCHEDULER_PERF_TEST)
  add_executable(arangodb_schedulerperftest EXCLUDE_FROM_ALL
    ThreadPoolPerfTest.cpp)
  target_include_directories(arangodb_schedulerperftest PRIVATE
    ${PROJECT_SOURCE_DIR}/tests
    ${PROJECT_SOURCE_DIR}/arangod)
  target_link_libraries(arangodb_schedulerperftest
    gtest
    gtest_main
    arango_tests_basics 
    arango_lightweight)
endif()
