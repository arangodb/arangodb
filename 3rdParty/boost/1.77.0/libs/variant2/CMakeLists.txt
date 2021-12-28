# Copyright 2018, 2019 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.16)

project(boost_variant2 VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

add_library(boost_variant2 INTERFACE)
add_library(Boost::variant2 ALIAS boost_variant2)

target_include_directories(boost_variant2 INTERFACE include)

target_link_libraries(boost_variant2
  INTERFACE
    Boost::config
    Boost::mp11
)

if(BUILD_TESTING)

  add_subdirectory(test)

endif()
