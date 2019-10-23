# Copyright 2018 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt
#
# NOTE: This does NOT run the unit tests for Boost.Atomic.
#       It only tests, if the CMakeLists.txt file in it's root works as expected

cmake_minimum_required( VERSION 3.5 )

project( BoostAtomicCMakeSelfTest )

add_definitions( -DBOOST_ALL_NO_LIB )

add_subdirectory( ../../../assert ${CMAKE_CURRENT_BINARY_DIR}/libs/assert )
add_subdirectory( ../../../config ${CMAKE_CURRENT_BINARY_DIR}/libs/config )
add_subdirectory( ../../../static_assert ${CMAKE_CURRENT_BINARY_DIR}/libs/static_assert )
add_subdirectory( ../../../type_traits ${CMAKE_CURRENT_BINARY_DIR}/libs/type_traits )

add_subdirectory( ../.. ${CMAKE_CURRENT_BINARY_DIR}/libs/boost_atomic )

add_executable( boost_atomic_cmake_self_test main.cpp )
target_link_libraries( boost_atomic_cmake_self_test Boost::atomic )
