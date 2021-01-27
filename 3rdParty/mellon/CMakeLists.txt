cmake_minimum_required(VERSION 3.16)
project(futures)

set(CMAKE_CXX_STANDARD 17)


add_library(mellon src/futures.cpp include/mellon/utilities.h include/mellon/completion-queue.h include/mellon/traits.h include/mellon/state-machine.h include/mellon/detail/box.h include/mellon/detail/invalid-pointer-flags.h include/mellon/detail/gadgets.h)
target_include_directories(mellon PUBLIC include)

option(MELLON_RECORD_BACKTRACE_ABANDONED_OBJECTS "Record the backtrace of abandoned objects" Off)

if (MELLON_RECORD_BACKTRACE_ABANDONED_OBJECTS)
    target_compile_definitions(mellon PUBLIC MELLON_RECORD_BACKTRACE)
    target_link_libraries(mellon unwind)
endif()

set(MELLON_GTEST_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/gtest")
option(MELLON_BUILD_TESTS "Build tests for futures using GTest" Off)
if (MELLON_BUILD_TESTS)
    add_subdirectory(${MELLON_GTEST_DIRECTORY})
    add_subdirectory(test)
endif()
