# Copyright 2018 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt
#
# NOTE: This does NOT run the unit tests for Boost.Preprocessor (yet).
#       It only tests, if the CMakeLists.txt file works as expected

cmake_minimum_required( VERSION 3.5 )

project( BoostPreprocessorCMakeSelfTest )

add_subdirectory( .. ${CMAKE_CURRENT_BINARY_DIR}/boost_preprocessor )

add_executable( boost_preprocessor_cmake_self_test config_info.cpp )
target_link_libraries( boost_preprocessor_cmake_self_test Boost::preprocessor )

