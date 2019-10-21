# Copyright 2018, 2019 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at http://boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.5)

project(CmakeConfigHeadersTest LANGUAGES CXX)

include(${CMAKE_CURRENT_LIST_DIR}/../BoostVersion.cmake)

find_package(Boost ${BOOST_VERSION} EXACT REQUIRED)

# Using `include_directories` is bad practice, done here for
# testing purposes. The right, "modern CMake", thing to do is
# `target_link_libraries(main Boost::headers)`.

include_directories(${Boost_INCLUDE_DIRS})

add_executable(main quick.cpp)

enable_testing()
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>)

add_test(main main)
