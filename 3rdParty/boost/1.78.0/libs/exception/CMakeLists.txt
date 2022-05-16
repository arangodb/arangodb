# Copyright 2020, 2021 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.20)

project(boost_exception VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

add_library(boost_exception STATIC
  src/clone_current_exception_non_intrusive.cpp
)

add_library(Boost::exception ALIAS boost_exception)

target_include_directories(boost_exception PUBLIC include)

target_link_libraries(boost_exception
  PUBLIC
    Boost::assert
    Boost::config
    Boost::core
    Boost::smart_ptr
    Boost::throw_exception
    Boost::tuple
    Boost::type_traits
)

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")

  add_subdirectory(test)

endif()
