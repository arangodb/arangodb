# Copyright 2020, 2021 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.16)

project(boost_chrono VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

add_library(boost_chrono
  src/chrono.cpp
  src/process_cpu_clocks.cpp
  src/thread_clock.cpp
)

add_library(Boost::chrono ALIAS boost_chrono)

target_include_directories(boost_chrono PUBLIC include)

target_link_libraries(boost_chrono
  PUBLIC
    Boost::assert
    Boost::config
    Boost::core
    Boost::integer
    Boost::move
    Boost::mpl
    Boost::predef
    Boost::ratio
    Boost::static_assert
    Boost::system
    Boost::throw_exception
    Boost::type_traits
    Boost::typeof
    Boost::utility
    Boost::winapi
)

target_compile_definitions(boost_chrono
  PUBLIC BOOST_CHRONO_NO_LIB
  # Source files already define BOOST_CHRONO_SOURCE
  # PRIVATE BOOST_CHRONO_SOURCE
)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(boost_chrono PUBLIC BOOST_CHRONO_DYN_LINK)
else()
  target_compile_definitions(boost_chrono PUBLIC BOOST_CHRONO_STATIC_LINK)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")

  add_subdirectory(test)

endif()
