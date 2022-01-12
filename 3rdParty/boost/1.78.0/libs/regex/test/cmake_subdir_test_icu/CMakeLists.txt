# Copyright 2018, 2019 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.16)

project(cmake_subdir_test LANGUAGES CXX)

add_subdirectory(../.. boostorg/regex)
add_subdirectory(../../../config boostorg/config)
add_subdirectory(../../../core boostorg/core)
add_subdirectory(../../../assert boostorg/assert)
add_subdirectory(../../../throw_exception boostorg/throw_exception)
add_subdirectory(../../../predef boostorg/predef)

add_executable(quick_icu ../quick_icu.cpp)
target_link_libraries(quick_icu Boost::regex_icu)

enable_testing()
add_test(quick_icu quick_icu)

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>)
