# Copyright 2020, 2021 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.16)

project(boost_random VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

add_library(boost_random
  src/random_device.cpp
)

add_library(Boost::random ALIAS boost_random)

target_include_directories(boost_random PUBLIC include)

target_link_libraries(boost_random
  PUBLIC
    Boost::array
    Boost::assert
    Boost::config
    Boost::core
    Boost::dynamic_bitset
    Boost::integer
    Boost::io
    Boost::range
    Boost::static_assert
    Boost::system
    Boost::throw_exception
    Boost::type_traits
    Boost::utility
)

target_compile_definitions(boost_random
  PUBLIC BOOST_RANDOM_NO_LIB
  # Source files already define BOOST_RANDOM_SOURCE
  # PRIVATE BOOST_RANDOM_SOURCE
)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(boost_random PUBLIC BOOST_RANDOM_DYN_LINK)
else()
  target_compile_definitions(boost_random PUBLIC BOOST_RANDOM_STATIC_LINK)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")

  add_subdirectory(test)

endif()

