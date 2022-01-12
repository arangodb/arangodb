# Copyright 2018-2021 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.20)

project(cmake_subdir_test LANGUAGES CXX)

add_subdirectory(../.. boostorg/timer)

set(deps

  # Primary dependencies
  chrono
  config
  core
  io
  predef
  system
  throw_exception

  # Secondary dependencies
  assert
  integer
  move
  mpl
  ratio
  static_assert
  type_traits
  typeof
  utility
  winapi
  variant2
  preprocessor
  rational
  mp11
)

foreach(dep IN LISTS deps)

  add_subdirectory(../../../${dep} boostorg/${dep})

endforeach()

add_executable(main main.cpp)
target_link_libraries(main Boost::timer Boost::core)

enable_testing()
add_test(main main)
