cmake_minimum_required(VERSION 3.17)
project(futures)

set(CMAKE_CXX_STANDARD 17)


add_library(mellon src/futures.cpp include/mellon/utilities.h include/mellon/completion-queue.h include/mellon/traits.h include/mellon/state-machine.h)
target_include_directories(mellon PUBLIC include)


set(FUTURES_GTEST_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/gtest")
option(FUTURES_BUILD_TESTS "Build tests for futures using GTest" Off)
if (FUTURES_BUILD_TESTS)
    add_subdirectory(${FUTURES_GTEST_DIRECTORY})
    add_subdirectory(test)
endif()
