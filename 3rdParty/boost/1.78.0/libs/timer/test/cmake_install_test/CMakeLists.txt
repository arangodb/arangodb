# Copyright 2018-2021 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.20)

project(cmake_install_test LANGUAGES CXX)

find_package(boost_timer REQUIRED)

add_executable(main main.cpp)
target_link_libraries(main Boost::timer)

enable_testing()
add_test(main main)
