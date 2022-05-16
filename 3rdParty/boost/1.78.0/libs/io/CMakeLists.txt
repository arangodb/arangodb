# Copyright 2018 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# (http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.5...3.20)

project(boost_io VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

add_library(boost_io INTERFACE)
add_library(Boost::io ALIAS boost_io)

target_include_directories(boost_io INTERFACE include)

target_link_libraries(boost_io
  INTERFACE
    Boost::config
)
