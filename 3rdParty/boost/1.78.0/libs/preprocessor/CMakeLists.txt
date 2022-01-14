# Copyright 2019 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.16)

project(boost_preprocessor VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

add_library(boost_preprocessor INTERFACE)
add_library(Boost::preprocessor ALIAS boost_preprocessor)

target_include_directories(boost_preprocessor INTERFACE include)

if(BOOST_SUPERPROJECT_VERSION)

  include(BoostInstall)
  boost_install(TARGETS boost_preprocessor HEADER_DIRECTORY include/)

endif()

if(BUILD_TESTING)

  add_subdirectory(test)

endif()
