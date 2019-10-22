# Copyright 2018 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt
#
# NOTE: This does NOT run the unit tests for Boost.Array.
#       It only tests, if the CMakeLists.txt file in it's root works as expected

cmake_minimum_required( VERSION 3.5 )

project( BoostBindCMakeSelfTest )

add_subdirectory( ../../../assert ${CMAKE_CURRENT_BINARY_DIR}/libs/assert )
add_subdirectory( ../../../config ${CMAKE_CURRENT_BINARY_DIR}/libs/config )
add_subdirectory( ../../../core ${CMAKE_CURRENT_BINARY_DIR}/libs/core )
add_subdirectory( ../../../static_assert ${CMAKE_CURRENT_BINARY_DIR}/libs/static_assert )
add_subdirectory( ../../../throw_exception ${CMAKE_CURRENT_BINARY_DIR}/libs/thorw_exception )

add_subdirectory( ../.. ${CMAKE_CURRENT_BINARY_DIR}/libs/boost_array )

add_executable( boost_array_cmake_self_test main.cpp )
target_link_libraries( boost_array_cmake_self_test Boost::array )

