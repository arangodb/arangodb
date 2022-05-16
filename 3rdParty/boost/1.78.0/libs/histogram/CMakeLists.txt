# Copyright Peter Dimov, Hans Dembinski 2018-2019
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

# We support CMake 3.5, but prefer 3.16 policies and behavior
cmake_minimum_required(VERSION 3.5...3.16)

project(boost_histogram VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

add_library(boost_histogram INTERFACE)
add_library(Boost::histogram ALIAS boost_histogram)

target_include_directories(boost_histogram INTERFACE include)
target_compile_features(boost_histogram INTERFACE cxx_std_14)
target_link_libraries(boost_histogram
  INTERFACE
    Boost::config
    Boost::core
    Boost::mp11
    Boost::throw_exception
    Boost::variant2
)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  # Standalone build, fetch dependencies

  # Fetch support files

  message(STATUS "Fetching BoostFetch.cmake")

  file(DOWNLOAD
    "https://raw.githubusercontent.com/boostorg/cmake/develop/include/BoostFetch.cmake"
    "${CMAKE_BINARY_DIR}/BoostFetch.cmake"
  )

  include("${CMAKE_BINARY_DIR}/BoostFetch.cmake")

  boost_fetch(boostorg/cmake TAG develop NO_ADD_SUBDIR)

  FetchContent_GetProperties(boostorg_cmake)

  list(APPEND CMAKE_MODULE_PATH ${boostorg_cmake_SOURCE_DIR}/include)

  # Enable testing

  include(CTest)
  add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>)
  add_dependencies(check tests) # needed to build the "run" tests

  if(BUILD_TESTING)

    set(BUILD_TESTING OFF) # do not build tests of dependencies

    boost_fetch(boostorg/static_assert TAG develop EXCLUDE_FROM_ALL)
    boost_fetch(boostorg/assert TAG develop EXCLUDE_FROM_ALL) # needed by core
    boost_fetch(boostorg/config TAG develop EXCLUDE_FROM_ALL)
    boost_fetch(boostorg/core TAG develop EXCLUDE_FROM_ALL)
    boost_fetch(boostorg/mp11 TAG develop EXCLUDE_FROM_ALL)
    boost_fetch(boostorg/throw_exception TAG develop EXCLUDE_FROM_ALL)
    boost_fetch(boostorg/variant2 TAG develop EXCLUDE_FROM_ALL)
    ## No cmake support yet
    # boost_fetch(boostorg/accumulators TAG develop)
    # boost_fetch(boostorg/range TAG develop)
    # boost_fetch(boostorg/serialization TAG develop)
    # boost_fetch(boostorg/units TAG develop)

    set(BUILD_TESTING ON)

  endif()

endif()

if (BUILD_TESTING)

  add_subdirectory(test)

  # do not pollute the superproject with the benchmarks
  if(NOT BOOST_SUPERPROJECT_VERSION)

    add_subdirectory(benchmark)

  endif()

endif()
