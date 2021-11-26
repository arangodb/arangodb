# Copyright 2018 Mike Dev
# Copyright 2019 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

# We support CMake 3.5, but prefer 3.16 policies and behavior
cmake_minimum_required(VERSION 3.5...3.16)

project(boost_regex VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

add_library(boost_regex INTERFACE)
add_library(Boost::regex ALIAS boost_regex)

target_include_directories(boost_regex INTERFACE include)

target_link_libraries(boost_regex
  INTERFACE
    Boost::config
    Boost::throw_exception
    Boost::predef
    Boost::assert
)

