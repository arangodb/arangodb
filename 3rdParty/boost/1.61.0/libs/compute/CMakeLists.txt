# ---------------------------------------------------------------------------
#  Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
#
#  Distributed under the Boost Software License, Version 1.0
#  See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt
#
# ---------------------------------------------------------------------------

cmake_minimum_required(VERSION 2.8)

project(BoostCompute)

set(CMAKE_MODULE_PATH ${BoostCompute_SOURCE_DIR}/cmake)
if (CMAKE_VERSION VERSION_LESS "3.1.0")
    list(APPEND CMAKE_MODULE_PATH "${BoostCompute_SOURCE_DIR}/cmake/opencl")
endif()

# find OpenCL
find_package(OpenCL REQUIRED)
include_directories(SYSTEM ${OpenCL_INCLUDE_DIRS})

# find Boost
find_package(Boost 1.48 REQUIRED)
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

# optional support for c++11
option(BOOST_COMPUTE_USE_CPP11 "Use C++11 features" OFF)
if(NOT MSVC)
  if(${BOOST_COMPUTE_USE_CPP11})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  endif()
endif()

# optional support for offline-caching
option(BOOST_COMPUTE_USE_OFFLINE_CACHE "Use offline cache for OpenCL program binaries" OFF)
if(${BOOST_COMPUTE_USE_OFFLINE_CACHE})
  add_definitions(-DBOOST_COMPUTE_USE_OFFLINE_CACHE)
endif()

# thread-safety options
option(BOOST_COMPUTE_THREAD_SAFE "Compile with BOOST_COMPUTE_THREAD_SAFE defined" OFF)
if(${BOOST_COMPUTE_THREAD_SAFE})
  add_definitions(-DBOOST_COMPUTE_THREAD_SAFE)
  if(${BOOST_COMPUTE_USE_CPP11})
    if(MSVC)
      if (MSVC_VERSION GREATER 1800)
        add_definitions(-DBOOST_COMPUTE_HAVE_THREAD_LOCAL)
      endif()
    else()
      add_definitions(-DBOOST_COMPUTE_HAVE_THREAD_LOCAL)
    endif()
  endif()
endif()

# optional third-party libraries
option(BOOST_COMPUTE_HAVE_EIGEN "Have Eigen" OFF)
option(BOOST_COMPUTE_HAVE_OPENCV "Have OpenCV" OFF)
option(BOOST_COMPUTE_HAVE_QT "Have Qt" OFF)
option(BOOST_COMPUTE_HAVE_VTK "Have VTK" OFF)
option(BOOST_COMPUTE_HAVE_CUDA "Have CUDA" OFF)
option(BOOST_COMPUTE_HAVE_TBB "Have TBB" OFF)
option(BOOST_COMPUTE_HAVE_BOLT "Have BOLT" OFF)

include_directories(include)

if(${OpenCL_HEADER_CL_EXT_FOUND})
  add_definitions(-DBOOST_COMPUTE_HAVE_HDR_CL_EXT)
endif()

if(WIN32)
  # optional support for boost dynamic libraries
  option(BOOST_COMPUTE_BOOST_ALL_DYN_LINK "Use boost dynamic link libraries" OFF)
  if(${BOOST_COMPUTE_BOOST_ALL_DYN_LINK})
    add_definitions(-DBOOST_ALL_DYN_LINK)
  else()
    # for default use statis libs
    set(Boost_USE_STATIC_LIBS ON)
  endif()
  link_directories(${Boost_LIBRARY_DIRS})
endif()

# compiler options
option(BOOST_COMPUTE_ENABLE_COVERAGE "Enable code coverage generation" OFF)

# Visual Studio C++
if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_definitions(-D_SCL_SECURE_NO_WARNINGS)
  add_definitions(-DNOMINMAX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4003") # Not enough actual parameters for a BOOST_PP macro
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4244") # Warning C4244: 'initializing': conversion from 'double' to 'int', possible loss of data
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4305") # Warning C4305: 'initializing': truncation from 'double' to 'float'
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4800") # Warning C4800: 'uint32_t' : forcing value to bool 'true' or 'false' (performance warning)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4838") # Warning C4838: conversion from 'double' to 'float' requires a narrowing conversion
endif()

option(BOOST_COMPUTE_BUILD_TESTS "Build the Boost.Compute tests" OFF)
if(${BOOST_COMPUTE_BUILD_TESTS})
  enable_testing()
  add_subdirectory(test)
endif()

option(BOOST_COMPUTE_BUILD_BENCHMARKS "Build the Boost.Compute benchmarks" OFF)
if(${BOOST_COMPUTE_BUILD_BENCHMARKS})
  add_subdirectory(perf)
endif()

option(BOOST_COMPUTE_BUILD_EXAMPLES "Build the Boost.Compute examples" OFF)
if(${BOOST_COMPUTE_BUILD_EXAMPLES})
  add_subdirectory(example)
endif()

# configure cmake config file
configure_file(
  cmake/BoostComputeConfig.cmake.in
  ${BoostCompute_BINARY_DIR}/BoostComputeConfig.cmake
  @ONLY
)

# install cmake config file
install(
  FILES ${BoostCompute_BINARY_DIR}/BoostComputeConfig.cmake
  DESTINATION lib/cmake/BoostCompute
)

# install header files
install(DIRECTORY include/boost DESTINATION include/compute)
