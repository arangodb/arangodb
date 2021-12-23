# Copyright 2018 Mike Dev
# Copyright 2019 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

# We support CMake 3.5, but prefer 3.16 policies and behavior
cmake_minimum_required(VERSION 3.5...3.16)

project(boost_static_assert VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

add_library(boost_static_assert INTERFACE)
add_library(Boost::static_assert ALIAS boost_static_assert)

target_include_directories(boost_static_assert INTERFACE include)

target_link_libraries(boost_static_assert
  INTERFACE
    Boost::config
)

# BUILD_TESTING is the standard CTest variable that enables testing

if(BUILD_TESTING)

  add_subdirectory(test)
  add_subdirectory(example)

endif()
