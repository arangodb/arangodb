# Copyright 2019 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.16)

project(boost_endian VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

add_library(boost_endian INTERFACE)
add_library(Boost::endian ALIAS boost_endian)

target_include_directories(boost_endian INTERFACE include)

target_link_libraries(boost_endian
  INTERFACE
    Boost::config
    Boost::core
    Boost::static_assert
    Boost::type_traits
)

if(BUILD_TESTING)

  add_subdirectory(test)

endif()
