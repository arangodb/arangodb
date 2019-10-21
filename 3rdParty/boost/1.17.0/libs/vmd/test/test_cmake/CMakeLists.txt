# Copyright 2018 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt
#
# NOTE: This does NOT run the unit tests for Boost.Vmd.
#       It only tests, if the CMakeLists.txt file in it's root works as expected

cmake_minimum_required( VERSION 3.5 )

project( BoostVmdCMakeSelfTest )

add_subdirectory( ../../../preprocessor ${CMAKE_CURRENT_BINARY_DIR}/libs/preprocessor )
add_subdirectory( ../.. ${CMAKE_CURRENT_BINARY_DIR}/libs/boost_vmd )

add_executable( boost_vmd_cmake_self_test main.cpp )
target_link_libraries( boost_vmd_cmake_self_test Boost::vmd )

