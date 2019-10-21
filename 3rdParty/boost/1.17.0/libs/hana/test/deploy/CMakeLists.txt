# Copyright Louis Dionne 2013-2017
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.0)
project(external CXX)
find_package(Hana REQUIRED)

add_executable(main main.cpp)
target_link_libraries(main hana)
